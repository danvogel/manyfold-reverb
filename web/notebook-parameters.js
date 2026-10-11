var ManyspaceNotebookParameters = {
  distance_ms: ["reflection spacing", "ms", 1, [15, 95], [15, 95], "how far apart the walls feel. 15 ms is a closet; 95 ms is a gymnasium.", 0, 1],
  speed: ["playback speed", "×", .01, [.5, 1.5], [.5, 1.5], "the room plays your sound back at a different speed. 0.5 is half-time, an octave down; 1.5 is a fifth up, like a tape sped up.", 1, 2],
  damping: ["damping", "", .01, [.2, .85], [.2, .85], "how fast the brightness fades from the tail. 0.2 is tile and glass; 0.85 is heavy curtains.", 2, 2],
  lowpass_hz: ["high cut", "Hz", 10, [800, 16000], [2500, 8000], "the highest sound that survives in the room. 800 Hz is like hearing it through a wall; 16 kHz is higher than older adults can hear.", -1, 0],
  highpass_hz: ["low cut", "Hz", 1, [10, 1000], [20, 40], "the lowest sound that survives. 10 Hz you feel more than hear; 1 kHz thins it to a small speaker.", -1, 0],
  spread: ["stereo spread", "%", 1, [0, 100], [35, 100], "how wide the room sounds. 0 is one speaker in front of you; 100 fills both ears.", -1, 0],
  diffusion: ["diffusion", "%", 1, [0, 100], [30, 80], "how smeared the echoes are. 0 is distinct slapbacks off a far wall; 100 is a wash.", -1, 0],
  predelay_ms: ["pre-delay", "ms", 1, [0, 120], [0, 35], "the pause before the room answers. 0 is immediate; 120 ms is a shout across a hall.", -1, 0],
  reflections: ["early reflections", "", 1, [0, 7], [1, 7], "the pattern of the first few echoes: off, corridor, even, cluster, scatter, pairs, wide, room.", -1, 0],
  modulation_ms: ["modulation depth", "ms", .1, [0, 6], [0, 1.2], "how far the tail drifts in pitch. 0 is steady; 6 ms is seasick.", -1, 1],
  modulation_hz: ["modulation rate", "Hz", .05, [.1, 1.5], [.15, .6], "how fast it drifts. 0.1 a second is a slow tide; 1.5 is a wobble.", -1, 2],
  shimmer: ["shimmer", "%", 1, [0, 40], [0, 12], "how much of the tail comes back an octave up. 0 is none; 40 is a choir of ghosts.", -1, 0],
  hold: ["hold", "%", 1, [0, 100], [0, 0], "freezes the tail so it rings on. 0 is off; 100 holds until the next note.", -1, 0],
  drive: ["loop drive", "×", .1, [1, 6], [1, 1.3], "how hard the tail is pushed. 1 is clean; 6 is a fuzz pedal.", -1, 1],
  ducking: ["ducking", "%", 1, [0, 80], [0, 35], "how much the tail steps aside while you play. 0 never; 80 is nearly silent until you stop.", -1, 0],
  pan: ["pan", "", .05, [-1, 1], [-.6, .6], "where the room sits, hard left (-1) to hard right (+1).", -1, 2],
  bloom_ms: ["bloom", "ms", 10, [0, 600], [0, 140], "how long the room takes to swell in. 0 is instant; 600 ms is a slow volume swell.", -1, 0]
};
