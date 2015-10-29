# amdctl
Set P-State voltages and clock speeds on recent AMD CPUs on Linux.

Use at your own risk, can damage your hardware if used incorrectly (if you raise the voltage too high for example).

Supports AMD CPU family's 10h, 11h, 12h, 15h, 16h, `cat /proc/cpuinfo` to see your CPU family.

Can be used to overclock/underclock/overvoltage/undervoltage most recent AMD CPU's (2007 - 2015).

Currently I've only tested on a 10h CPU, I will be getting a 16h CPU to test shorty.  
Other CPU families's should work, I've read the BKDG manuals for all the CPU families and adjusted the code accordingly.

Requires root access.

Requires the msr kernel module.

To manually load the module: `sudo modprobe msr`

To automatically load the module see the [arch wiki](https://wiki.archlinux.org/index.php/Kernel_modules#Automatic_module_handling).

References: 

http://developer.amd.com/resources/documentation-articles/developer-guides-manuals/  
https://wiki.archlinux.org/index.php/K10ctl  
http://sourceforge.net/projects/k10ctl/  
https://web.archive.org/web/20090914081440/http://www.ztex.de/misc/k10ctl.e.html  
https://01.org/msr-tools  
https://en.wikipedia.org/wiki/List_of_AMD_CPU_microarchitectures  
http://users.atw.hu/instlatx64/  
