/*
 * isp.c
 *
 * Hand the chip to its ROM bootloader on request, so new firmware can be loaded over the
 * same USB cable with blhost and nobody has to find the bootload jumper (JP1 pulls the ISP
 * pin, PIO0_5, low for the ROM to sample at reset). NXP's AN13183 names this entry point
 * for the LPC55(S)1x: the ROM's runBootloader(), reached through the API tree that the
 * SDK's fsl_iap driver wraps as BOOTLOADER_UserEntry(). The option word is laid out in
 * fsl_iap.h: tag 0xEB marks a valid request, mode 1 forces ISP (0 would just re-run the
 * normal boot decision), interface 1 is USB-HID (0 lets the ROM poll every port).
 */
#include "fsl_device_registers.h"
#include "fsl_common.h"
#include "fsl_clock.h"
#include "fsl_power.h"
#include "fsl_iap.h"
#include "usb_port.h"
#include "isp.h"

/* Top of the main stack: vector 0, placed by the linker script at the end of SRAM. */
extern uint32_t _vStackTop;

void IspOnMainStack(void) __attribute__((noinline, noreturn, used));

/*
 * Runs on the main stack, which sits at the very top of SRAM (0x2000B400..0x2000C000):
 * above the first 26 KB the ROM bootloader takes for its own data (UM11295 lists it as
 * 0x30000000..0x30006800, the secure alias), and above the FreeRTOS heap where the task
 * stacks live. The option word is a local here ON PURPOSE: this frame is built after the
 * stack switch, so its address is right, it is in RAM in case the ROM writes it back, and
 * it is outside the region the ROM clears.
 */
void IspOnMainStack(void){
	user_app_boot_invoke_option_t option = {0};

	option.option.B.tag = 0xEBU;
	option.option.B.mode = 1U;
	option.option.B.boot_interface = 1U;

	/* Detach from the host so it sees a new device, not a CDC port that stopped answering. */
	USBStop();

	/* The ROM builds its clock tree from the reset default, the FRO 12 MHz main clock, not
	 * from the 150 MHz PLL the application runs on -- so put the main clock back there (the
	 * first half of BOARD_BootClockFRO12M). Deliberately NOT the second half: that drops the
	 * core voltage to the 12 MHz profile and the flash wait states to 1, and if the ROM then
	 * raises the clock without setting its own, flash reads go wrong. The 150 MHz settings are
	 * safe at any lower clock; too few wait states are not. */
	POWER_DisablePD(kPDRUNCFG_PD_FRO192M);
	CLOCK_SetupFROClocking(12000000U);
	CLOCK_AttachClk(kFRO12M_to_MAIN_CLK);
	CLOCK_SetupFROClocking(96000000U);
	CLOCK_SetClkDiv(kCLOCK_DivAhbClk, 1U, false);
	SystemCoreClock = 12000000U;

	BOOTLOADER_UserEntry(&option);

	/* Never reached when the ROM accepts the request. If it hands back, come up as the
	 * application instead of sitting here with every interrupt off. */
	NVIC_SystemReset();
	for(;;){}
}

/*
 * Move thread mode from the task's process stack to a fresh main stack and jump to
 * IspOnMainStack. This has to be assembly with no C around it: GCC addresses every local
 * and spill through sp, and an `msr CONTROL` inside a C function silently changes which
 * register sp names -- the first build of this file stored the option word on the task
 * stack, switched, then handed the ROM an address computed from the NEW stack (MSP+4, one
 * word past the end of SRAM). A naked function may hold only basic asm. MSPLIM is cleared
 * first because the startup code fences the main stack at 3 KB and the ROM may run deeper.
 * CONTROL is written as 0, not just with SPSEL cleared: FPCA would otherwise carry a task's
 * FPU context into the ROM, which then stacks 104-byte frames on every exception instead
 * of the 32-byte ones a cold boot gives it (legal in thread mode, where no lazy FP store
 * can be outstanding); nPRIV is already 0, tasks run privileged in this port.
 * r0 = top of the main stack.
 */
static void __attribute__((naked, noreturn)) IspSwitchToMainStack(uint32_t msp_top){
	__asm(
		".syntax unified\n"
		"movs  r1, #0\n"
		"msr   msplim, r1\n"
		"msr   msp, r0\n"
		"msr   control, r1\n"	/* SPSEL = 0: thread mode now runs on MSP; FPCA = 0; nPRIV = 0 */
		"isb\n"
		"b     IspOnMainStack\n"
	);
}

void IspEnterRomBootloader(void){
	unsigned int i;

	/* Quiesce the application so nothing can take the CPU away before the ROM has it, but
	 * leave it looking like a fresh reset rather than a masked one: every interrupt SOURCE is
	 * switched off and PRIMASK/BASEPRI end up clear, because the ROM enables the interrupts
	 * it needs and may not clear a global mask it never set. The tick goes first so the
	 * scheduler cannot preempt this task; then every NVIC line, so no ISR can post to the
	 * kernel either. */
	__disable_irq();
	SysTick->CTRL = 0U;
	for(i = 0; i < ARRAY_SIZE(NVIC->ICER); i++){
		NVIC->ICER[i] = 0xFFFFFFFFU;
		NVIC->ICPR[i] = 0xFFFFFFFFU;
	}
	/* The NVIC registers do not reach the system handlers: a tick that counted down between
	 * the cpsid above and the CTRL store is still pended in ICSR and would run the scheduler
	 * the moment interrupts come back. Clear it, and PendSV, explicitly. */
	SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
	__DSB();
	__ISB();
	__set_BASEPRI(0U);
	__enable_irq();

	IspSwitchToMainStack((uint32_t)&_vStackTop);
}
