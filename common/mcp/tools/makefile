all: show_queue_proj tcpdump_parser_proj MirrorShm_proj echosvr_proj presscall_proj

show_queue_proj:
	@cd show_queue && make clean && make 

tcpdump_parser_proj:
	@cd tcpdump_parser && make clean && make

MirrorShm_proj:
	@cd MirrorShm && make clean && make

echosvr_proj:
	@cd echosvr && make clean && make

presscall_proj:
	@cd presscall && make clean && make

clean:
	@cd show_queue && make clean
	@cd tcpdump_parser && make clean
	@cd MirrorShm && make clean
	@cd echosvr && make clean
	@cd presscall && make clean

