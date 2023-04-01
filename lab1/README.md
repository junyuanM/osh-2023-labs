# phase3实验过程
创建一个新内存磁盘，将initrd.c作为init.c打包进去，运行即可输出结果
# 选做：分析哪些选项能够切实地影响编译后的内核大小或编译的时间
Processor tyoe and features下：

删除Check for low memory corruption、
EFI runtime service support、
Build a relocatable kernel
后bzImage内存少了大约3.5M，编译时间减少了20%左右

Enable loadable module support的内存大小为3M，
删除之后编译时间减少约20%

Mitigations for speculative execution vulnerabilities的大小为0.9M

Power management and ACPI options的大小为0.6M

将Device Drivers中的
PCI support、
PCCard support、
Block devices、
Serial ATA、
Multiple devices driver support、
Macintosh device drivers、
PPS support、
Hardware Monitoring support、
Thermal drivers
删除后内存节省了1.9M，并且编译时间减少约30%
