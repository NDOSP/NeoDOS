module main;

import bootinfo;

bootinfo::BootInfo info @section(".bootinfo"); 

fn void kernel_main() @export("kernel_main") {
	for (;;) {
		asm("hlt");
	}
}