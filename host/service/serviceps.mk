
serviceps.dll: dlldata.obj service_p.obj service_i.obj
	link /dll /out:serviceps.dll /def:serviceps.def /entry:DllMain dlldata.obj service_p.obj service_i.obj \
		kernel32.lib rpcndr.lib rpcns4.lib rpcrt4.lib oleaut32.lib uuid.lib \

.c.obj:
	cl /c /Ox /DWIN32 /D_WIN32_WINNT=0x0400 /DREGISTER_PROXY_DLL \
		$<

clean:
	@del serviceps.dll
	@del serviceps.lib
	@del serviceps.exp
	@del dlldata.obj
	@del service_p.obj
	@del service_i.obj
