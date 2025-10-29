cls

del /Q *.spv *.bak *.txt *.res *.obj *.exe

glslangValidator.exe -V -H -o shader_Scene0.vert.spv shader_Scene0.vert

glslangValidator.exe -V -H -o shader_Scene0.frag.spv shader_Scene0.frag

glslangValidator.exe -V -H -o Shader_Scene1.vert.spv Shader_Scene1.vert

glslangValidator.exe -V -H -o Shader_Scene1.frag.spv Shader_Scene1.frag

glslangValidator.exe -V -H -o Shader_Scene2.vert.spv Shader_Scene2.vert

glslangValidator.exe -V -H -o Shader_Scene2.frag.spv Shader_Scene2.frag

glslangValidator.exe -V -H -o Shader_Scene3.vert.spv Shader_Scene3.vert

glslangValidator.exe -V -H -o Shader_Scene3.frag.spv Shader_Scene3.frag 

cl.exe /c /EHsc /I C:\VulkanSDK\Anjaneya\Include SceneSwitcher.cpp Scene0.cpp Scene1.cpp Scene2.cpp Scene3.cpp

rc.exe Scene0.rc

link.exe  SceneSwitcher.obj Scene0.obj Scene1.obj Scene2.obj Scene3.obj Scene0.res /LIBPATH:C:\VulkanSDK\Anjaneya\Lib /SUBSYSTEM:WINDOWS

SceneSwitcher.exe


