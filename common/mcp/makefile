all: ccs_proj scc_proj

ccs_proj:
	@cd ccs/src && make cleanall && make
	#rm -f release/mcp/ccs/bin/*
	#cp ccs/bin/* release/mcp/ccs/bin/
	#rm -f release/mcp/ccs/etc/*
	#cp ccs/etc/* release/mcp/ccs/etc/

scc_proj:
	@cd scc/src && make cleanall && make
	#rm -f release/mcp/scc/bin/*
	#cp scc/bin/* release/mcp/scc/bin/
	#rm -f release/mcp/scc/etc/*
	#cp scc/etc/* release/mcp/scc/etc/
	
clean:
	@cd ccs/src && make clean
	@cd scc/src && make clean
	rm -f ./public/*.o
	rm -f ./smcp/src/*.o

