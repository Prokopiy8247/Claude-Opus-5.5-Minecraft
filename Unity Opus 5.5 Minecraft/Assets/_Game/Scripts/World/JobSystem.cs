using System;
using System.Collections.Concurrent;
using System.Threading;
using UnityEngine;

namespace MCR
{
    /// <summary>Minimal worker-thread pool with two priority queues.</summary>
    public sealed class JobSystem : IDisposable
    {
        readonly Thread[] threads;
        readonly ConcurrentQueue<Action> high = new ConcurrentQueue<Action>();
        readonly ConcurrentQueue<Action> normal = new ConcurrentQueue<Action>();
        readonly SemaphoreSlim signal = new SemaphoreSlim(0);
        volatile bool running = true;
        int inFlight;
        public int InFlight => Volatile.Read(ref inFlight);
        public int QueuedNormal => normal.Count;
        public int QueuedHigh => high.Count;
        public readonly int ThreadCount;
        public static JobSystem Instance;

        public JobSystem(int count)
        {
            ThreadCount = Math.Max(1, count);
            threads = new Thread[ThreadCount];
            for (int i = 0; i < ThreadCount; i++)
            {
                threads[i] = new Thread(Worker) { IsBackground = true, Name = "MCR-Worker-" + i, Priority = System.Threading.ThreadPriority.BelowNormal };
                threads[i].Start();
            }
            Instance = this;
        }

        public void Enqueue(Action a, bool highPriority = false)
        {
            Interlocked.Increment(ref inFlight);
            if (highPriority) high.Enqueue(a); else normal.Enqueue(a);
            signal.Release();
        }

        void Worker()
        {
            while (running)
            {
                signal.Wait(200);
                if (!running) break;
                Action a;
                while (running && (high.TryDequeue(out a) || normal.TryDequeue(out a)))
                {
                    try { a(); }
                    catch (Exception e) { Debug.LogError("[Job] " + e); }
                    finally { Interlocked.Decrement(ref inFlight); }
                    if (!high.IsEmpty) continue;
                }
            }
        }

        /// <summary>Run all queued work on the calling thread (used by batch/editor tools).</summary>
        public void DrainSync(int maxMillis = 60000)
        {
            var sw = System.Diagnostics.Stopwatch.StartNew();
            while (InFlight > 0 && sw.ElapsedMilliseconds < maxMillis)
            {
                Action a;
                if (high.TryDequeue(out a) || normal.TryDequeue(out a))
                {
                    try { a(); } catch (Exception e) { Debug.LogError("[Job] " + e); }
                    finally { Interlocked.Decrement(ref inFlight); }
                }
                else Thread.Sleep(1);
            }
        }

        public void Dispose()
        {
            running = false;
            for (int i = 0; i < ThreadCount; i++) signal.Release();
            foreach (var t in threads) { try { t.Join(500); } catch { } }
            if (Instance == this) Instance = null;
        }
    }
}
