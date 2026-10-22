# hollow_saint 0.7.0 host results (2026-10-22T19:22Z)

## tests/host.py phrase (SDK harness phrase, ASan+UBSan)
peak 14808 (-6.9 dBFS), non-zero samples 105966
phrase: knobs [16384, 16384, 12000, 12000] -> /home/user/work/cathedral/build/host/phrase.wav (2.4 s), exit 0

## tests/host.py suite (ASan+UBSan, firmware simulation)
scenario     peak    dBFS      rms      dc  clicks notes
phrase      19098    -4.7     3447    -0.2       0 
single      11640    -9.0     2552    -1.7       0 hold rms 2684 -> 2989 (x1.11)
chord6      25199    -2.3     4989    -2.9       0 hold rms 5888 -> 4891 (x0.83)
range       18116    -5.1     3129     9.7       0 
sweep0      18919    -4.8     3533    -2.1       0 
sweep1      13523    -7.7     3558    -1.0       0 
sweep2      14177    -7.3     3352    -1.4       0 
sweep3      13794    -7.5     3072    -1.7       0 
steal       20665    -4.0     3425     0.3       0 
extreme     17863    -5.3     3539     2.2       0 
vibrato     10873    -9.6     2561    -1.2       0 
long        14845    -6.9     2869    -2.0       0 hold rms 3032 -> 2691 (x0.89)
handover    22110    -3.4     3417    -0.2       0 
tail         9516   -10.7     1266    -0.8       0 
relic       23034    -3.1     4365    -3.5       0 relic vs dry: HF share x0.59, level x1.08
rtoggle     21557    -3.6     4632    -4.0       0 
relicage    13190    -7.9     3671    -1.0       0 
reliclong    13233    -7.9     2419    -1.3       0 hold rms 2744 -> 2731 (x1.00)
tremor      12036    -8.7     1944    -0.2       0 pulse max/min at 1/4 (steady): 25%:1.64 (1.28) 50%:1.88 (1.38) 89%:2.49 (1.58)
crankrelic    11695    -8.9     1813     0.3       0 
all checks passed

## tests/host.py cost (valgrind instructions per voice-block in render)
hollow_saint   instr/voice-block  mid   17872  max   19664  zero   11301  worst   19664
eva            instr/voice-block  mid   17286  max   18029  zero   16044  worst   18029  = 1.00 x eva worst
oxbow          instr/voice-block  mid    8809  max    9078  zero    8837  worst    9078  = 0.50 x eva worst
taffy          instr/voice-block  mid   19189  max   11135  zero   14845  worst   19189  = 1.06 x eva worst
wavetable      instr/voice-block  mid   20639  max   20639  zero   20639  worst   20639  = 1.14 x eva worst
hollow_saint worst = 1.09 x eva worst
