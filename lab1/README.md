#phase3实验过程
创建一个新内存磁盘，将initrd.c作为init.c打包进去，运行即可输出结果
#选做：分析哪些选项能够切实地影响编译后的内核大小或编译的时间
Processor tyoe and features下：
裁剪Enable 5-level page tables support会减小
NUMA Memory Allocation and Scheduler Support
Check for low memory corruption
Memory Protection Keys大小
EFI runtime service support
Build a relocatable kernel
General steup下：
Auditing support
Control Group support

Enable loadable module support的内存大小为3M
Mitigations for speculative....的大小为0.9M
Power management and ACPI options的大小为0.6M
将Device Drivers中的
PCI support
PCCard support
Block devices
Serial ATA
Multiple devices driver support
Macintosh device drivers
PPS support
Hardware Monitoring support
Thermal drivers
删除后内存节省了1.9M，并且编译时间减少了
