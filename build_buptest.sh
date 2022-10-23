gcc buptest.c gpio/ps_protocol.c -I./ -o buptest -march=armv8-a -mfloat-abi=hard -mfpu=neon-fp-armv8 -O0
gcc ataritest.c gpio/ps_protocol.c -I./ -o ataritest -march=armv8-a -mfloat-abi=hard -mfpu=neon-fp-armv8 -O0
