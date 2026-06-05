#ifndef INTHANDLERS_H
#define INTHANDLERS_H

#include "idt.h"
#include "string.h"
#include "video.h"

typedef void (*InterruptHandler)(INTERRUPT_FRAME*);

void registerInterruptHandler(uint8_t n, InterruptHandler h);
InterruptHandler getInterruptHandler(uint8_t n);

void pageFaultHandler(INTERRUPT_FRAME* frame);

void handlerScreen();
void defaultHandler(INTERRUPT_FRAME* frame);
#endif // INTHANDLERS_H