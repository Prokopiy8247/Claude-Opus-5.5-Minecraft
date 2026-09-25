using UnityEngine;

namespace MCR
{
    /// <summary>
    /// Creates the persistent game object on startup, so the game runs from any scene (the shipped scene is empty
    /// apart from this component). Also parses the command-line switches used by the automated checks.
    /// </summary>
    public sealed class Bootstrap : MonoBehaviour
    {
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void AutoCreate()
        {
            if (GameManager.Instance != null) return;
            if (Object.FindFirstObjectByType<GameManager>() != null) return;
            var go = new GameObject("MCR Game");
            go.AddComponent<GameManager>();
            go.AddComponent<AutoTest>();
        }

        void Awake()
        {
            if (GameManager.Instance == null && GetComponent<GameManager>() == null) gameObject.AddComponent<GameManager>();
            if (GetComponent<AutoTest>() == null) gameObject.AddComponent<AutoTest>();
        }
    }
}
