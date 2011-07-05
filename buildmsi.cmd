@echo off
candle -d"Manufacturer=Vinay Sajip" -d"Platform=x86" -d"Version=1.0.0.0" launch_module.wxs
light launch_module.wixobj
del launch_module.wixobj
candle -d"Manufacturer=Vinay Sajip" -d"Platform=x86" -d"Version=1.0.0.0" launcher.wxs
light -ext WixUIExtension launcher.wixobj
del launcher.wixobj

candle -d"Manufacturer=Vinay Sajip" -d"Platform=x64" -d"Version=1.0.0.0" launch_module.wxs
light -o launch_module.amd64.msm launch_module.wixobj
del launch_module.wixobj
candle -d"Manufacturer=Vinay Sajip" -d"Platform=x64" -d"Version=1.0.0.0" launcher.wxs
light -o launcher.amd64.msi -ext WixUIExtension launcher.wixobj
del launcher.wixobj
