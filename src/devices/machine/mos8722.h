// license:BSD-3-Clause
// copyright-holders:Curt Coder
/**********************************************************************

    MOS Technology 8722 Memory Management Unit emulation

**********************************************************************
                            _____   _____
                   Vdd   1 |*    \_/     | 48  SENSE40
                _RESET   2 |             | 47  (MS3) 128/64
                  TA15   3 |             | 46  _EXROM
                  TA14   4 |             | 45  _GAME
                  TA13   5 |             | 44  FSDIR
                  TA12   6 |             | 43  _Z80EN
                  TA11   7 |             | 42  D7
                  TA10   8 |             | 41  D6
                   TA9   9 |             | 40  D5
                   TA8  10 |             | 39  D4
                 _CAS1  11 |             | 38  D3
                 _CAS0  12 |   MOS8722   | 37  D2
         I/O SEL (MS2)  13 |             | 36  D1
        ROMBANK1 (MS1)  14 |             | 35  D0
        ROMBANK0 (MS0)  15 |             | 34  Vss
                   AEC  16 |             | 33  phi0
                   MUX  17 |             | 32  R/_W
                    A0  18 |             | 31  A15
                    A1  19 |             | 30  A14
                    A2  20 |             | 29  A13
                    A3  21 |             | 28  A12
                 A4/A5  22 |             | 27  A11
                 A6/A7  23 |             | 26  A10
                    A8  24 |_____________| 25  A9

**********************************************************************/

#ifndef MAME_MACHINE_MOS8722_H
#define MAME_MACHINE_MOS8722_H

#pragma once

//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> mos8722_device

class mos8722_device :  public device_t
{
public:
	// construction/destruction
	mos8722_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	auto z80en() { return m_write_z80en.bind(); }
	auto fsdir() { return m_write_fsdir.bind(); }
	auto game() { return m_read_game.bind(); }
	auto exrom() { return m_read_exrom.bind(); }
	auto sense40() { return m_read_sense40.bind(); }

	uint8_t read(offs_t offset, uint8_t data);
	void write(offs_t offset, uint8_t data);

	int fsdir_r();

	offs_t ta_r(offs_t offset, int aec, int *ms0, int *ms1, int *ms2, int *ms3, int *cas0, int *cas1);

protected:
	// device-level overrides
	virtual void device_start() override ATTR_COLD;
	virtual void device_reset() override ATTR_COLD;

private:
	enum
	{
		CR = 0,
		PCRA, LCRA = PCRA,
		PCRB, LCRB = PCRB,
		PCRC, LCRC = PCRC,
		PCRD, LCRD = PCRD,
		MCR,
		RCR,
		P0L,
		P0H,
		P1L,
		P1H,
		VR
	};

	enum
	{
		CR_IO_SYSTEM_IO = 0,
		CR_IO_HI_ROM
	};

	enum
	{
		CR_ROM_SYSTEM_ROM = 0,
		CR_ROM_INT_FUNC_ROM,
		CR_ROM_EXT_FUNC_ROM,
		CR_ROM_RAM
	};

	enum
	{
		RCR_SHARE_1K = 0,
		RCR_SHARE_4K,
		RCR_SHARE_8K,
		RCR_SHARE_16K
	};

	devcb_write_line   m_write_z80en;
	devcb_write_line   m_write_fsdir;
	devcb_read_line    m_read_game;
	devcb_read_line    m_read_exrom;
	devcb_read_line    m_read_sense40;

	uint8_t m_reg[16];

	uint8_t m_p0h_latch;
	uint8_t m_p1h_latch;
};


// device type definition
DECLARE_DEVICE_TYPE(MOS8722, mos8722_device)

#endif // MAME_MACHINE_MOS8722_H
