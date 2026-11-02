// Reads interleaved float32 stereo from argv[1], runs the C++ core with the given settings, writes argv[2].
// usage: lap_parity in.f32 out.f32 peak gain limit emph mix
#include "../src/core/LapCat2A.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
int main (int argc, char** argv)
{
    if (argc < 8) return 2;
    FILE* f = std::fopen (argv[1], "rb"); if (! f) return 3;
    std::vector<float> buf; float tmp[4096]; size_t n;
    while ((n = std::fread (tmp, sizeof (float), 4096, f)) > 0) buf.insert (buf.end(), tmp, tmp + n);
    std::fclose (f);
    const size_t frames = buf.size() / 2;
    std::vector<float> l (frames), r (frames);
    for (size_t i = 0; i < frames; ++i) { l[i] = buf[2 * i]; r[i] = buf[2 * i + 1]; }
    lap::Core c; c.prepare (48000.0);
    c.setOversampling (false); c.setDcBlock (false); // the mockup model has neither a DC block nor a latency-exact oversampler
    lap::Params p; p.peak = (float) std::atof (argv[3]); p.gain = (float) std::atof (argv[4]); p.limit = (float) std::atof (argv[5]);
    p.emph = (float) std::atof (argv[6]); p.mix = (float) std::atof (argv[7]);
    c.setParams (p); c.snap();
    // odd block sizes on purpose
    size_t pos = 0; const int blocks[] = { 64, 100, 511, 7, 333 }; int bi = 0;
    while (pos < frames) { size_t k = std::min<size_t> ((size_t) blocks[bi++ % 5], frames - pos); c.process (l.data() + pos, r.data() + pos, (int) k); pos += k; }
    FILE* o = std::fopen (argv[2], "wb"); if (! o) return 4;
    for (size_t i = 0; i < frames; ++i) { std::fwrite (&l[i], 4, 1, o); std::fwrite (&r[i], 4, 1, o); }
    std::fclose (o);
    return 0;
}
