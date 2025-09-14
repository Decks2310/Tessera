#pragma once

namespace EmbeddedSVGs
{
    // Use Raw String Literals R"SVG(...)SVG" for embedding.
    // The base color is #000000 (black), which allows for dynamic recoloring in the UI.
    // 1. Distortion
    static const char* distortionData = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">
  <title>Distortion</title>
  <path d="M4 32 C 10 20, 18 20, 24 32 C 30 44, 32 44, 36 32 L 40 18 L 44 46 L 48 22 L 52 42 L 56 30 L 60 34"
        fill="none" stroke="#000000" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
)SVG";

    // 2. Filter
    static const char* filterData = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">
  <title>Filter</title>
  <path d="M 4 20 H 30 C 40 20, 44 24, 48 32 C 52 40, 56 48, 60 52"
        fill="none" stroke="#000000" stroke-width="6" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
)SVG";

    // 3. Modulation
    static const char* modulationData = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">
  <title>Modulation</title>
  <path d="M 4 32 C 16 16, 24 16, 32 32 C 40 48, 48 48, 60 32"
        fill="none" stroke="#000000" stroke-width="4" stroke-linecap="round"/>
  <path d="M 4 32 C 16 48, 24 48, 32 32 C 40 16, 48 16, 60 32"
        fill="none" stroke="#000000" stroke-width="4" stroke-linecap="round" opacity="0.6"/>
</svg>
)SVG";

    // 4. Delay
    static const char* delayData = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">
  <title>Delay</title>
  <ellipse cx="14" cy="32" rx="10" ry="16" fill="#000000" opacity="1"/>
  <ellipse cx="34" cy="32" rx="8" ry="14" fill="#000000" opacity="0.6"/>
  <ellipse cx="50" cy="32" rx="6" ry="12" fill="#000000" opacity="0.3"/>
</svg>
)SVG";

    // 5. Reverb
    static const char* reverbData = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">
  <title>Reverb</title>
  <path d="M 24 50 C 36 44, 36 20, 24 14" fill="none" stroke="#000000" stroke-width="4" stroke-linecap="round" opacity="1"/>
  <path d="M 34 56 C 48 48, 48 16, 34 8" fill="none" stroke="#000000" stroke-width="4" stroke-linecap="round" opacity="0.7"/>
  <path d="M 44 62 C 60 52, 60 12, 44 2" fill="none" stroke="#000000" stroke-width="4" stroke-linecap="round" opacity="0.4"/>
</svg>
)SVG";

    // 6. Compressor
    static const char* compressorData = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">
  <title>Compressor</title>
  <path d="M4 18 H 24 L 28 26 H 36 L 40 18 H 60" fill="none" stroke="#000000" stroke-width="4" stroke-linejoin="round"/>
  <path d="M4 46 H 24 L 28 38 H 36 L 40 46 H 60" fill="none" stroke="#000000" stroke-width="4" stroke-linejoin="round"/>
  <path d="M 32 10 L 32 16 M 28 13 L 32 17 L 36 13" fill="none" stroke="#000000" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M 32 54 L 32 48 M 28 51 L 32 47 L 36 51" 
 fill="none" stroke="#000000" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
)SVG";

    // 7. ChromaTape
    static const char* chromaTapeData = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">
  <title>ChromaTape</title>
  <circle cx="32" cy="32" r="28" fill="none" stroke="#000000" stroke-width="4"/>
  <circle cx="32" cy="32" r="8" fill="#000000"/>
  <rect x="12" y="20" width="40" height="6" fill="#000000" opacity="0.4"/>
  <rect x="12" y="30" width="40" height="6" fill="#000000" opacity="0.7"/>
  <rect x="12" y="40" width="40" height="6" fill="#000000" opacity="1.0"/>
</svg>
)SVG";

    // 8. MorphoComp
    static const char* morphoCompData = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">
  <title>MorphoComp</title>
  <path d="M 4 32 L 20 32 L 32 20 L 44 44 L 60 32" fill="none" stroke="#000000" stroke-width="5" stroke-linecap="round" stroke-linejoin="round" opacity="1"/>
  <path d="M 4 32 C 16 20, 24 20, 32 32 C 40 44, 48 44, 60 32" fill="none" stroke="#000000" stroke-width="3" stroke-linecap="round" stroke-linejoin="round" opacity="0.5" stroke-dasharray="5 5"/>
</svg>
)SVG";

    // 9. Physical Resonator (Using Dice Icon as placeholder)
    static const char* diceData = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">
  <title>Random</title>
  <rect x="10" y="10" width="44" height="44" rx="8" fill="none" stroke="#000000" stroke-width="4"/>
  <circle cx="22" cy="22" r="4" fill="#000000"/>
  <circle cx="42" cy="22" r="4" fill="#000000"/>
  <circle cx="32" cy="32" r="4" fill="#000000"/>
  <circle cx="22" cy="42" r="4" fill="#000000"/>
  <circle cx="42" cy="42" r="4" fill="#000000"/>
</svg>
)SVG";

    // 10. Spectral Animator
    static const char* spectralAnimatorData = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">
  <title>Spectral Animator</title>
  <path d="M 8 56 V 8 L 56 8 V 56 Z" fill="none" stroke="#000000" stroke-width="4" stroke-linejoin="round"/>
  <path d="M 16 48 V 24 L 24 40 L 32 16 L 40 44 L 48 20 V 48" fill="none" stroke="#000000" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
)SVG";

    // 11. Helical Delay
    static const char* helicalDelayData = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">
  <title>Helical Delay</title>
  <path d="M12 4C12 16 24 16 24 28S12 40 12 52" fill="none" stroke="#000000" stroke-width="4" stroke-linecap="round"/>
  <path d="M40 12C40 24 52 24 52 36S40 48 40 60" fill="none" stroke="#000000" stroke-width="4" stroke-linecap="round" opacity="0.6"/>
  <path d="M16 32h32" fill="none" stroke="#000000" stroke-width="2" stroke-linecap="round" stroke-dasharray="4 4"/>
</svg>
)SVG";

    // 12. Chrono-Verb
    static const char* chronoVerbData = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">
  <title>Chrono-Verb</title>
  <line x1="4" y1="32" x2="60" y2="32" stroke="#000000" stroke-width="2" opacity="0.5"/>
  <line x1="32" y1="4" x2="32" y2="60" stroke="#000000" stroke-width="2" opacity="0.5"/>
  <path d="M 32,32 m -24,0 a 24,24 0 1,0 48,0 a 24,24 0 1,0 -48,0"
        fill="none" stroke="#000000" stroke-width="3" stroke-linecap="round" stroke-dasharray="8 10" transform="rotate(15 32 32)"/>
  <path d="M 32,32 m -16,0 a 16,16 0 1,0 32,0 a 16,16 0 1,0 -32,0"
        fill="none" stroke="#000000" stroke-width="3.5" stroke-linecap="round" stroke-dasharray="1 8" transform="rotate(-30 32 32)"/>
  <path d="M 32,32 m -8,0 a 8,8 0 1,0 16,0 a 8,8 0 1,0 -16,0"
        fill="none" stroke="#000000" stroke-width="4" stroke-linecap="round"/>
</svg>
)SVG";

    // 13. Tectonic Delay
    static const char* tectonicDelayData = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">
  <title>Tectonic Delay</title>
  <!-- Three frequency bands represented by layered triangular "tectonic" shapes -->
  <!-- Low band (bottom layer) -->
  <path d="M 8 52 L 20 40 L 32 52 L 20 52 Z" fill="#000000" opacity="1.0"/>
  <!-- Mid band (middle layer) -->
  <path d="M 20 40 L 32 28 L 44 40 L 32 40 Z" fill="#000000" opacity="0.7"/>
  <!-- High band (top layer) -->
  <path d="M 32 28 L 44 16 L 56 28 L 44 28 Z" fill="#000000" opacity="0.4"/>
  
  <!-- Synthesis texture lines emanating from each band -->
  <path d="M 14 46 L 10 50 M 18 46 L 22 50 M 26 46 L 30 50" stroke="#000000" stroke-width="2" opacity="0.8"/>
  <path d="M 26 34 L 22 38 M 30 34 L 34 38 M 38 34 L 42 38" stroke="#000000" stroke-width="2" opacity="0.6"/>
  <path d="M 38 22 L 34 26 M 42 22 L 46 26 M 50 22 L 54 26" stroke="#000000" stroke-width="2" opacity="0.4"/>
</svg>
)SVG";
}