lib/$(BLD_CONFIG)/libinmmd5.a: obj/$(BLD_CONFIG)/inm_md5.o
	mkdir -p lib/$(BLD_CONFIG)
	ar r lib/$(BLD_CONFIG)/libinmmd5.a obj/$(BLD_CONFIG)/inm_md5.o
	ranlib lib/$(BLD_CONFIG)/libinmmd5.a
obj/$(BLD_CONFIG)/inm_md5.o: inm_md5.cpp inm_md5.h
	mkdir -p obj/$(BLD_CONFIG)
	g++ $(CFLAGS) -c inm_md5.cpp -o obj/$(BLD_CONFIG)/inm_md5.o
clean:
	rm -rf lib obj
