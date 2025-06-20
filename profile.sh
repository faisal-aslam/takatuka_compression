make profile
./compress-profile tests/testText2.txt
gprof ./compress-profile gmon.out > profile.txt


