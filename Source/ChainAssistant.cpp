// Presets de estilo, asistente de texto (reglas por palabras clave) y analisis de la voz.
#include "ChainProcessor.h"
#include <algorithm>
#include <cmath>

namespace
{
    struct Style
    {
        const char* name;
        float gain, lowCut, mud, pres, air;      // Gain, EQ
        float deess, comp, makeup;                // De-esser, compresor
        float satDrive, satMix;                   // Saturacion
        float revMix, revSize;                    // Reverb
        float delMix, delTime, delFb;             // Delay
    };

    const Style styles[] = {
        { "Pop limpio",      0.5f, 0.50f, 0.30f, 0.30f, 0.35f,  0.50f, 0.45f, 0.50f,  0.15f, 0.40f,  0.15f, 0.40f,  0.05f, 0.30f, 0.25f },
        { "Rap / Trap",      0.5f, 0.55f, 0.35f, 0.40f, 0.30f,  0.60f, 0.70f, 0.55f,  0.30f, 0.50f,  0.10f, 0.30f,  0.12f, 0.35f, 0.30f },
        { "Podcast",         0.5f, 0.60f, 0.40f, 0.35f, 0.10f,  0.50f, 0.60f, 0.60f,  0.10f, 0.30f,  0.00f, 0.20f,  0.00f, 0.30f, 0.20f },
        { "Calido intimo",   0.5f, 0.45f, 0.20f, 0.15f, 0.15f,  0.40f, 0.40f, 0.45f,  0.40f, 0.60f,  0.20f, 0.50f,  0.05f, 0.30f, 0.25f },
        { "Radio brillante", 0.5f, 0.50f, 0.30f, 0.50f, 0.55f,  0.70f, 0.60f, 0.55f,  0.25f, 0.40f,  0.12f, 0.35f,  0.08f, 0.30f, 0.25f },
        { "Ambiental",       0.5f, 0.45f, 0.25f, 0.25f, 0.40f,  0.50f, 0.40f, 0.50f,  0.20f, 0.40f,  0.35f, 0.70f,  0.25f, 0.45f, 0.40f },
        // Interpretacion propia por genero (reggaeton melodico / urbano electronico). NO son cadenas reales de ningun artista.
        { "Reggaeton melodico", 0.5f, 0.55f, 0.35f, 0.40f, 0.45f,  0.60f, 0.65f, 0.55f,  0.25f, 0.45f,  0.20f, 0.45f,  0.15f, 0.35f, 0.35f },
        { "Urbano electronico", 0.5f, 0.60f, 0.35f, 0.35f, 0.50f,  0.65f, 0.70f, 0.55f,  0.35f, 0.50f,  0.15f, 0.40f,  0.20f, 0.30f, 0.35f },
    };

    juce::String stripAccents (juce::String s)
    {
        const char* from[] = { "\xc3\xa1", "\xc3\xa9", "\xc3\xad", "\xc3\xb3", "\xc3\xba", "\xc3\xb1" };
        const char* to[]   = { "a", "e", "i", "o", "u", "n" };
        for (int i = 0; i < 6; ++i) s = s.replace (juce::String::fromUTF8 (from[i]), to[i]);
        return s;
    }

    void nudge (BuiltinEffect* fx, int knob, float delta)
    {
        if (fx != nullptr) fx->setKnob (knob, fx->getKnob (knob) + delta);
    }
}

juce::StringArray ChainProcessor::getStyleNames()
{
    juce::StringArray n;
    for (auto& s : styles) n.add (s.name);
    return n;
}

void ChainProcessor::loadStyle (const juce::String& name)
{
    const Style* st = &styles[0];
    for (auto& s : styles) if (name == s.name) st = &s;

    clearChain();
    auto set = [] (BuiltinEffect* fx, std::initializer_list<float> v)
    {
        int i = 0;
        for (float x : v) fx->setKnob (i++, x);
    };
    set (static_cast<BuiltinEffect*> (addBuiltin (BuiltinEffect::Gain)->proc.get()),    { st->gain });
    set (static_cast<BuiltinEffect*> (addBuiltin (BuiltinEffect::Eq)->proc.get()),      { st->lowCut, st->mud, st->pres, st->air });
    set (static_cast<BuiltinEffect*> (addBuiltin (BuiltinEffect::DeEsser)->proc.get()), { st->deess });
    set (static_cast<BuiltinEffect*> (addBuiltin (BuiltinEffect::Comp)->proc.get()),    { st->comp, st->makeup });
    set (static_cast<BuiltinEffect*> (addBuiltin (BuiltinEffect::Sat)->proc.get()),     { st->satDrive, st->satMix });
    set (static_cast<BuiltinEffect*> (addBuiltin (BuiltinEffect::Reverb)->proc.get()),  { st->revMix, st->revSize });
    set (static_cast<BuiltinEffect*> (addBuiltin (BuiltinEffect::Delay)->proc.get()),   { st->delMix, st->delTime, st->delFb });
}

// ---------------------------------------------------------------- asistente de texto
juce::String ChainProcessor::runAssistant (const juce::String& raw)
{
    using FX = BuiltinEffect;
    auto text = stripAccents (raw.toLowerCase().trim());
    if (text.isEmpty()) return {};

    text = text.replace (" y ", ",").replace (" pero ", ",");
    const auto clauses = juce::StringArray::fromTokens (text, ",;.!?", "");
    juce::StringArray done;

    for (auto clause : clauses)
    {
        clause = " " + clause.trim() + " ";
        if (clause.trim().isEmpty()) continue;

        auto any = [&clause] (std::initializer_list<const char*> words)
        {
            for (auto w : words) if (clause.contains (w)) return true;
            return false;
        };

        const bool less = any ({ "menos", "sin ", "quita", "baja", "reduce", "apaga", "elimina" });
        const float dir = less ? -1.0f : 1.0f;
        float amt = 0.15f;
        if (any ({ "mucho", "bastante", " muy " })) amt *= 2.0f;
        if (any ({ "poco", "poquito", "ligeramente", "sutil" })) amt *= 0.5f;

        bool matched = false;

        // ---- presets por estilo ----
        if (any ({ "preset", "estilo", "modo" }))
        {
            const char* name = nullptr;
            if (any ({ " pop" }))                                         name = "Pop limpio";
            else if (any ({ "reggaeton", "melodic" }))                    name = "Reggaeton melodico";
            else if (any ({ "electron" }))                                name = "Urbano electronico";
            else if (any ({ " rap", "trap", "drill", "urbano" }))         name = "Rap / Trap";
            else if (any ({ "podcast", "locucion", "hablado" }))          name = "Podcast";
            else if (any ({ "intimo", "intima", "susurr", "calido" }))    name = "Calido intimo";
            else if (any ({ "radio" }))                                   name = "Radio brillante";
            else if (any ({ "ambient", "etereo" }))                       name = "Ambiental";
            if (name != nullptr) { loadStyle (name); done.add (T ("Cargué el estilo: ") + name); continue; }
        }
        if (any ({ "reinicia", "reset", "desde cero" }))
        {
            loadStyle ("Pop limpio"); done.add (T ("Cadena reiniciada (Pop limpio)")); continue;
        }

        // ---- brillo ----
        if (any ({ "brillan", "brillo", " aire", "claridad", " claro", "presencia", "nitid" }))
        {
            auto* eq = findBuiltin (FX::Eq, true);
            nudge (eq, 3, dir * amt); nudge (eq, 2, dir * amt * 0.7f);
            done.add (dir > 0 ? T ("Más brillo y presencia (EQ)") : T ("Menos brillo y presencia (EQ)")); matched = true;
        }
        if (any ({ "oscur", "opac", "apagad" }))
        {
            auto* eq = findBuiltin (FX::Eq, true);
            nudge (eq, 3, -dir * amt); nudge (eq, 2, -dir * amt * 0.7f);
            done.add (T ("Sonido más oscuro (EQ)")); matched = true;
        }
        // ---- calidez ----
        if (any ({ "calid", "caliente", "grueso", "analog", "satur" }))
        {
            nudge (findBuiltin (FX::Sat, true), 0, dir * amt);
            nudge (findBuiltin (FX::Sat, true), 1, dir * amt * 0.5f);
            nudge (findBuiltin (FX::Eq, true),  3, -dir * amt * 0.3f);
            done.add (dir > 0 ? T ("Más calidez (saturación)") : T ("Menos saturación")); matched = true;
        }
        // ---- sibilancia (problema -> siempre corrige) ----
        if (any ({ "sibilan", " eses", "siseo", "silb" }))
        {
            nudge (findBuiltin (FX::DeEsser, true), 0, amt * 1.5f);
            done.add (T ("Más de-esser (menos sibilancia)")); matched = true;
        }
        else if (any ({ "deess", "de-ess", "de ess" }))
        {
            nudge (findBuiltin (FX::DeEsser, true), 0, dir * amt * 1.5f);
            done.add (T ("Ajusté el de-esser")); matched = true;
        }
        // ---- barro ----
        if (any ({ "barro", "turbi", "sucio", "embarrad" }))
        {
            auto* eq = findBuiltin (FX::Eq, true);
            nudge (eq, 1, amt * 1.5f); nudge (eq, 0, amt * 0.3f);
            done.add (T ("Quité barro (EQ 300 Hz)")); matched = true;
        }
        // ---- graves ----
        if (any ({ "grave", "bajos", "cuerpo", " peso" }))
        {
            nudge (findBuiltin (FX::Eq, true), 0, -dir * amt);
            done.add (dir > 0 ? T ("Más cuerpo (low cut más bajo)") : T ("Menos graves (low cut más alto)")); matched = true;
        }
        // ---- compresion ----
        if (any ({ "comprim", "pegad", "control", "parejo", "constante", "uniform" }))
        {
            nudge (findBuiltin (FX::Comp, true), 0, dir * amt * 1.3f);
            done.add (dir > 0 ? T ("Más compresión") : T ("Menos compresión")); matched = true;
        }
        // ---- reverb / eco ----
        if (any ({ "reverb", "espacio", "ambien", " sala", "profund" }))
        {
            nudge (findBuiltin (FX::Reverb, true), 0, dir * amt * 1.2f);
            done.add (dir > 0 ? T ("Más reverb") : T ("Menos reverb")); matched = true;
        }
        if (any ({ " seca ", " seco " }))
        {
            nudge (findBuiltin (FX::Reverb, true), 0, -0.3f);
            nudge (findBuiltin (FX::Delay, true), 0, -0.3f);
            done.add (T ("Voz más seca (menos reverb y delay)")); matched = true;
        }
        if (any ({ " eco ", "delay", "rebote" }))
        {
            nudge (findBuiltin (FX::Delay, true), 0, dir * amt);
            done.add (dir > 0 ? T ("Más delay") : T ("Menos delay")); matched = true;
        }
        // ---- volumen ----
        if (any ({ "volumen", "ganancia", " nivel" }))
        {
            nudge (findBuiltin (FX::Gain, true), 0, dir * amt * 0.5f);
            done.add (dir > 0 ? T ("Subí el volumen de entrada") : T ("Bajé el volumen de entrada")); matched = true;
        }

        if (! matched) done.add (T ("(no entendí: \"") + clause.trim() + "\")");
    }

    if (done.isEmpty()) return {};
    juce::String out;
    for (auto& d : done) out << "- " << d << "\n";
    if (out.contains (T ("no entendí")))
        out << T ("Prueba con: \"más brillante\", \"menos sibilancia\", \"más reverb\", \"más comprimido\", "
                  "\"más cálido\", \"quita el barro\", \"preset rap\".");
    return out.trimEnd();
}

// ---------------------------------------------------------------- escuchar la voz
bool ChainProcessor::startListening()
{
    if (capture.getNumSamples() == 0) return false;
    capturePos = 0;
    captureReady = false;
    listening = true;
    return true;
}

float ChainProcessor::getListenProgress() const
{
    const int cap = capture.getNumSamples();
    return cap > 0 ? (float) capturePos.load() / (float) cap : 0.0f;
}

juce::String ChainProcessor::buildFromCapture()
{
    captureReady = false;
    const int N = 4096;
    const int total = capture.getNumSamples();
    const float* x = capture.getReadPointer (0);

    juce::dsp::FFT fft (12);
    juce::dsp::WindowingFunction<float> window ((size_t) N, juce::dsp::WindowingFunction<float>::hann);
    std::vector<float> data ((size_t) N * 2), power ((size_t) N / 2, 0.0f);
    int frames = 0;

    for (int start = 0; start + N <= total; start += N / 2)
    {
        double e = 0.0;
        for (int i = 0; i < N; ++i) e += (double) x[start + i] * x[start + i];
        if (std::sqrt (e / N) < 0.003) continue;            // silencio

        std::fill (data.begin(), data.end(), 0.0f);
        std::copy (x + start, x + start + N, data.begin());
        window.multiplyWithWindowingTable (data.data(), (size_t) N);
        fft.performFrequencyOnlyForwardTransform (data.data());
        for (size_t k = 0; k < (size_t) N / 2; ++k) power[k] += data[k] * data[k];
        ++frames;
    }
    if (frames < 4)
        return T ("No oí suficiente voz. Reproduce o graba tu voz en el DAW mientras escucho y vuelve a probar.");

    auto bandDb = [&] (double lo, double hi)
    {
        hi = juce::jmin (hi, sr * 0.45);
        double sum = 0.0; int cnt = 0;
        for (int k = 1; k < N / 2; ++k)
        {
            const double f = k * sr / N;
            if (f >= lo && f < hi) { sum += power[(size_t) k]; ++cnt; }
        }
        return 10.0 * std::log10 (sum / juce::jmax (1, cnt) / frames + 1.0e-12);
    };

    const double mid  = bandDb (500, 2000);
    const double low  = bandDb (80, 200);
    const double mudD = bandDb (200, 450) - mid;
    const double presD = bandDb (2500, 5000) - mid;
    const double sibD = bandDb (5500, 9000) - mid;
    const double airD = bandDb (9500, 14000) - mid;
    const double lowD = low - mid;

    // dinamica: RMS cada 50 ms
    std::vector<float> rmsDb;
    const int blk = (int) (0.05 * sr);
    for (int s = 0; s + blk <= total; s += blk)
    {
        double e = 0.0;
        for (int i = 0; i < blk; ++i) e += (double) x[s + i] * x[s + i];
        const float db = juce::Decibels::gainToDecibels ((float) std::sqrt (e / blk), -100.0f);
        if (db > -50.0f) rmsDb.push_back (db);
    }
    std::sort (rmsDb.begin(), rmsDb.end());
    const float p10 = rmsDb.empty() ? -40.0f : rmsDb[rmsDb.size() / 10];
    const float p50 = rmsDb.empty() ? -25.0f : rmsDb[rmsDb.size() / 2];
    const float p90 = rmsDb.empty() ? -15.0f : rmsDb[rmsDb.size() * 9 / 10];
    const float spread = p90 - p10;

    auto cl = [] (double v, double lo, double hi) { return (float) juce::jlimit (lo, hi, v); };
    const float gainK  = cl (0.5 + juce::jlimit (-12.0, 12.0, -18.0 - (double) p50) / 24.0, 0.0, 1.0);
    const float lowCut = lowD > 6.0 ? 0.6f : 0.45f;
    const float mudK   = cl ((mudD - 2.0) / 10.0, 0.0, 1.0);
    const float presK  = cl (0.2 + (-presD - 8.0) / 16.0, 0.05, 0.7);
    const float airK   = cl (0.2 + (-airD - 20.0) / 24.0, 0.05, 0.7);
    const float deK    = cl (0.3 + (sibD + 14.0) / 16.0, 0.15, 1.0);
    const float compK  = cl (0.25 + (spread - 8.0) / 20.0, 0.2, 0.9);

    clearChain();
    auto fx = [this] (BuiltinEffect::Type t) { return static_cast<BuiltinEffect*> (addBuiltin (t)->proc.get()); };
    fx (BuiltinEffect::Gain)->setKnob (0, gainK);
    auto* eq = fx (BuiltinEffect::Eq);
    eq->setKnob (0, lowCut); eq->setKnob (1, mudK); eq->setKnob (2, presK); eq->setKnob (3, airK);
    fx (BuiltinEffect::DeEsser)->setKnob (0, deK);
    auto* cp = fx (BuiltinEffect::Comp); cp->setKnob (0, compK); cp->setKnob (1, 0.5f);
    auto* st = fx (BuiltinEffect::Sat);  st->setKnob (0, 0.2f);  st->setKnob (1, 0.4f);
    auto* rv = fx (BuiltinEffect::Reverb); rv->setKnob (0, 0.15f); rv->setKnob (1, 0.4f);
    auto* dl = fx (BuiltinEffect::Delay);  dl->setKnob (0, 0.08f); dl->setKnob (1, 0.3f); dl->setKnob (2, 0.25f);

    juce::String r;
    r << T ("Analicé tu voz y armé una cadena a medida:\n");
    r << T ("- Nivel medio ") << juce::String (p50, 1) << " dB -> entrada ajustada\n";
    r << T ("- Barro (200-450 Hz) ") << juce::String (mudD, 1) << " dB vs medios -> mud cut " << juce::String (mudK, 2) << "\n";
    r << T ("- Sibilancia (5.5-9 kHz) ") << juce::String (sibD, 1) << " dB -> de-esser " << juce::String (deK, 2) << "\n";
    r << T ("- Variación de nivel ") << juce::String (spread, 1) << " dB -> compresor " << juce::String (compK, 2) << "\n";
    r << T ("Es un punto de partida: afínalo con el chat o abriendo cada efecto.");
    return r;
}
