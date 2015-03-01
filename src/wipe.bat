@echo off
@attrib /r /s /h *.suo
@for /r %%x in (*.user;*.scc;*.suo;*.ncb) do (
	@echo %%x
	@del %%x
)

