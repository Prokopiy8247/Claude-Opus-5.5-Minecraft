using System.Collections.Generic;
using UnityEngine;

namespace MCR
{
    /// <summary>Common interface colours, matching the game's grey-panel look with original pixel art.</summary>
    public static class Styles
    {
        public static readonly Color32 Text = new Color32(255, 255, 255, 255);
        public static readonly Color32 TextDim = new Color32(170, 170, 170, 255);
        public static readonly Color32 TextGray = new Color32(120, 120, 120, 255);
        public static readonly Color32 Shadow = new Color32(0, 0, 0, 190);
        public static readonly Color32 Panel = new Color32(198, 198, 198, 255);
        public static readonly Color32 PanelDark = new Color32(138, 138, 138, 255);
        public static readonly Color32 PanelLight = new Color32(255, 255, 255, 255);
        public static readonly Color32 SlotBg = new Color32(139, 139, 139, 255);
        public static readonly Color32 SlotDark = new Color32(55, 55, 55, 255);
        public static readonly Color32 SlotHighlight = new Color32(255, 255, 255, 130);
        public static readonly Color32 TooltipBg = new Color32(16, 0, 16, 240);
        public static readonly Color32 TooltipBorderTop = new Color32(80, 0, 255, 255);
        public static readonly Color32 TooltipBorderBottom = new Color32(47, 0, 159, 255);
        public static readonly Color32 Button = new Color32(143, 143, 143, 255);
        public static readonly Color32 ButtonHi = new Color32(196, 196, 196, 255);
        public static readonly Color32 ButtonDisabled = new Color32(96, 96, 96, 255);
        public static readonly Color32 Green = new Color32(85, 255, 85, 255);
        public static readonly Color32 Yellow = new Color32(255, 255, 85, 255);
        public static readonly Color32 Red = new Color32(255, 85, 85, 255);
        public static readonly Color32 Gold = new Color32(255, 170, 0, 255);
        public static readonly Color32 Aqua = new Color32(85, 255, 255, 255);
        public static readonly Color32 LightPurple = new Color32(255, 85, 255, 255);
        public static readonly Color32 Blue = new Color32(85, 85, 255, 255);
        public static readonly Color32 DarkGray = new Color32(85, 85, 85, 255);

        public static Color32 RarityColor(Rarity r)
        {
            switch (r)
            {
                case Rarity.Uncommon: return Yellow;
                case Rarity.Rare: return Aqua;
                case Rarity.Epic: return LightPurple;
                default: return Text;
            }
        }

        /// <summary>Name colours used by the § formatting codes that recipes and item tooltips already emit.</summary>
        public static Color32 ColorCode(char c, Color32 fallback)
        {
            switch (c)
            {
                case '0': return new Color32(0, 0, 0, 255);
                case '1': return new Color32(0, 0, 170, 255);
                case '2': return new Color32(0, 170, 0, 255);
                case '3': return new Color32(0, 170, 170, 255);
                case '4': return new Color32(170, 0, 0, 255);
                case '5': return new Color32(170, 0, 170, 255);
                case '6': return Gold;
                case '7': return TextGray;
                case '8': return DarkGray;
                case '9': return Blue;
                case 'a': return Green;
                case 'b': return Aqua;
                case 'c': return Red;
                case 'd': return LightPurple;
                case 'e': return Yellow;
                case 'f': return Text;
                default: return fallback;
            }
        }

        /// <summary>Strip § codes for measuring and for cases that render plain text.</summary>
        public static string StripCodes(string s)
        {
            if (string.IsNullOrEmpty(s) || s.IndexOf('§') < 0) return s;
            var sb = new System.Text.StringBuilder(s.Length);
            for (int i = 0; i < s.Length; i++)
            {
                if (s[i] == '§' && i + 1 < s.Length) { i++; continue; }
                sb.Append(s[i]);
            }
            return sb.ToString();
        }
    }
}
