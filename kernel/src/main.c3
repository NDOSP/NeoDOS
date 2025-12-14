module main;

import bootinfo;

bootinfo::BootInfo info @section(".bootinfo"); 

fn void _start() @export("_start") {
	for (;;) {
		asm("hlt");
	}
}