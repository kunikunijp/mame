// license:BSD-3-Clause
// copyright-holders: Dan Boris, Aaron Giles

/***************************************************************************

    Atari Return of the Jedi hardware

    driver by Dan Boris

    Games supported:
        * Return of the Jedi

    Notes:
        * The schematics show the smoothing PROMs as being twice as large,
          but the current sizes are confirmed via a PCB.  The PROMs
          are 82S137 devices.

****************************************************************************

    Memory map

****************************************************************************

    ========================================================================
    CPU #1
    ========================================================================
    0000-07FF   R/W   xxxxxxxx    Z-page Working RAM
    0800-08FF   R/W   xxxxxxxx    NVRAM
    0C00        R     xxxx-xxx    Switch inputs #1
                R     x-------       (right coin)
                R     -x------       (left coin)
                R     --x-----       (aux coin)
                R     ---x----       (self test)
                R     -----x--       (left thumb switch)
                R     ------x-       (fire switches)
                R     -------x       (right thumb switch)
    0C01        R     xxx--x--    Communications
                R     x-------       (VBLANK)
                R     -x------       (sound CPU communications latch full flag)
                R     --x-----       (sound CPU acknowledge latch flag)
                R     -----x--       (slam switch)
    1400        R     xxxxxxxx    Sound acknowledge latch
    1800        R     xxxxxxxx    Read A/D conversion
    1C00          W   --------    Enable NVRAM
    1C01          W   --------    Disable NVRAM
    1C80          W   --------    Start A/D conversion (horizontal)
    1C82          W   --------    Start A/D conversion (vertical)
    1D00          W   --------    NVRAM store
    1D80          W   --------    Watchdog clear
    1E00          W   --------    Interrupt acknowledge
    1E80          W   x-------    Left coin counter
    1E81          W   x-------    Right coin counter
    1E82          W   x-------    LED 1 (not used)
    1E83          W   x-------    LED 2 (not used)
    1E84          W   x-------    Alphanumerics bank select
    1E86          W   x-------    Sound CPU reset
    1E87          W   x-------    Video off
    1F00          W   xxxxxxxx    Sound communications latch
    1F80          W   -----xxx    Program ROM bank select
    2000-23FF   R/W   xxxxxxxx    Scrolling playfield (low 8 bits)
    2400-27FF   R/W   ----xxxx    Scrolling playfield (upper 4 bits)
    2800-2BFF   R/W   xxxxxxxx    Color RAM low
                R/W   -----xxx       (blue)
                R/W   --xxx---       (green)
                R/W   xx------       (red LSBs)
    2C00-2FFF   R/W   ----xxxx    Color RAM high
                R/W   -------x       (red MSB)
                R/W   ----xxx-       (intensity)
    3000-37BF   R/W   xxxxxxxx    Alphanumerics RAM
    37C0-37EF   R/W   xxxxxxxx    Motion object picture
    3800-382F   R/W   -xxxxxxx    Motion object flags
                R/W   -x---xx-       (picture bank)
                R/W   --x-----       (vertical flip)
                R/W   ---x----       (horizontal flip)
                R/W   ----x---       (32 pixels tall)
                R/W   -------x       (X position MSB)
    3840-386F   R/W   xxxxxxxx       (Y position)
    38C0-38EF   R/W   xxxxxxxx       (X position LSBs)
    3C00-3C01     W   xxxxxxxx    Scrolling playfield vertical position
    3D00-3D01     W   xxxxxxxx    Scrolling playfield horizontal position
    3E00-3FFF     W   xxxxxxxx    PIXI graphics expander RAM
    4000-7FFF   R     xxxxxxxx    Banked program ROM
    8000-FFFF   R     xxxxxxxx    Fixed program ROM
    ========================================================================
    Interrupts:
        NMI not connected
        IRQ generated by 32V
    ========================================================================


    ========================================================================
    CPU #2
    ========================================================================
    0000-07FF   R/W   xxxxxxxx    Z-page working RAM
    0800-083F   R/W   xxxxxxxx    Custom I/O
    1000          W   --------    Interrupt acknowledge
    1100          W   xxxxxxxx    Speech data
    1200          W   --------    Speech write strobe on
    1300          W   --------    Speech write strobe off
    1400          W   xxxxxxxx    Main CPU acknowledge latch
    1500          W   -------x    Speech chip reset
    1800        R     xxxxxxxx    Main CPU communication latch
    1C00        R     x-------    Speech chip ready
    1C01        R     xx------    Communications
                R     x-------       (sound CPU communication latch full flag)
                R     -x------       (sound CPU acknowledge latch full flag)
    8000-FFFF   R     xxxxxxxx    Program ROM
    ========================================================================
    Interrupts:
        NMI not connected
        IRQ generated by 32V
    ========================================================================

***************************************************************************/

#include "emu.h"

#include "cpu/m6502/m6502.h"
#include "machine/74259.h"
#include "machine/adc0808.h"
#include "machine/nvram.h"
#include "machine/gen_latch.h"
#include "machine/rescap.h"
#include "machine/watchdog.h"
#include "machine/x2212.h"
#include "sound/pokey.h"
#include "sound/tms5220.h"

#include "emupal.h"
#include "screen.h"
#include "speaker.h"

#include <algorithm>


namespace {

#define DEBUG_GFXDECODE 0 // GFX layout for debug

class jedi_state : public driver_device
{
public:
	jedi_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_backgroundram(*this, "backgroundram"),
		m_foregroundram(*this, "foregroundram"),
		m_spriteram(*this, "spriteram"),
		m_smoothing_table(*this, "smoothing_table"),
		m_tx_gfx(*this, "tx_gfx"),
		m_bg_gfx(*this, "bg_gfx"),
		m_spr_gfx(*this, "spr_gfx"),
		m_proms(*this, "proms"),
		m_maincpu(*this, "maincpu"),
		m_audiocpu(*this, "audiocpu"),
		m_soundlatch(*this, "soundlatch"),
		m_sacklatch(*this, "sacklatch"),
		m_tms(*this, "tms"),
		m_novram(*this, "novram12%c", 'b'),
#if DEBUG_GFXDECODE
		m_gfxdecode(*this, "gfxdecode"),
#endif
		m_palette(*this, "palette"),
		m_screen(*this, "screen"),
		m_mainbank(*this, "mainbank")
	{ }

	ioport_value audio_comm_stat_0c01_r();
	void jedi(machine_config &config);

protected:
	virtual void machine_start() override ATTR_COLD;
	virtual void machine_reset() override ATTR_COLD;
	virtual void video_start() override ATTR_COLD;

private:
	required_shared_ptr<u8> m_backgroundram;
	required_shared_ptr<u8> m_foregroundram;
	required_shared_ptr<u8> m_spriteram;
	required_shared_ptr<u8> m_smoothing_table;
	required_region_ptr<u8> m_tx_gfx;
	required_region_ptr<u8> m_bg_gfx;
	required_region_ptr<u8> m_spr_gfx;
	required_region_ptr<u8> m_proms;
	required_device<cpu_device> m_maincpu;
	required_device<cpu_device> m_audiocpu;
	required_device<generic_latch_8_device> m_soundlatch;
	required_device<generic_latch_8_device> m_sacklatch;
	required_device<tms5220_device> m_tms;
	required_device_array<x2212_device, 2> m_novram;
#if DEBUG_GFXDECODE
	required_device<gfxdecode_device> m_gfxdecode;
	std::unique_ptr<u8[]> m_gfxdata;
#endif
	required_device<palette_device> m_palette;
	required_device<screen_device> m_screen;
	required_memory_bank m_mainbank;

	u32 m_vscroll = 0;
	u32 m_hscroll = 0;
	bool m_foreground_bank = false;
	bool m_video_off = false;

	emu_timer *m_interrupt_timer = nullptr;

	void main_irq_ack_w(u8 data);
	void rom_banksel_w(u8 data);
	template <uint8_t Which> void coin_counter_w(int state);
	u8 novram_data_r(address_space &space, offs_t offset);
	void novram_data_w(offs_t offset, u8 data);
	void novram_recall_w(offs_t offset, u8 data);
	void novram_store_w(u8 data);
	void vscroll_w(offs_t offset, u8 data);
	void hscroll_w(offs_t offset, u8 data);
	void irq_ack_w(u8 data);
	void audio_reset_w(int state);
	u8 audio_comm_stat_r();
	void speech_strobe_w(offs_t offset, u8 data);
	u8 speech_ready_r();
	void speech_reset_w(u8 data);
	void foreground_bank_w(int state);
	void video_off_w(int state);
	u32 screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);
	TIMER_CALLBACK_MEMBER(generate_interrupt);
	static rgb_t jedi_IRGB_3333(u32 raw);
	void do_pen_lookup(bitmap_rgb32 &bitmap, const rectangle &cliprect);
	void draw_background_and_text(bitmap_rgb32 &bitmap, const rectangle &cliprect);
	void draw_sprites(bitmap_rgb32 &bitmap, const rectangle &cliprect);

	void audio_map(address_map &map) ATTR_COLD;
	void main_map(address_map &map) ATTR_COLD;
};


/*************************************
 *
 *  Interrupt handling
 *
 *************************************/

void jedi_state::irq_ack_w(u8 data)
{
	m_audiocpu->set_input_line(M6502_IRQ_LINE, CLEAR_LINE);
}



/*************************************
 *
 *  Main CPU -> Sound CPU communications
 *
 *************************************/

void jedi_state::audio_reset_w(int state)
{
	m_audiocpu->set_input_line(INPUT_LINE_RESET, state ? CLEAR_LINE : ASSERT_LINE);
	if (!state)
		m_tms->set_output_gain(ALL_OUTPUTS, 0.0);
}


u8 jedi_state::audio_comm_stat_r()
{
	return (m_soundlatch->pending_r() << 7) | (m_sacklatch->pending_r() << 6);
}


ioport_value jedi_state::audio_comm_stat_0c01_r()
{
	return (m_soundlatch->pending_r() << 1) | m_sacklatch->pending_r();
}



/*************************************
 *
 *  Speech access
 *
 *************************************/

void jedi_state::speech_strobe_w(offs_t offset, u8 data)
{
	m_tms->wsq_w(BIT(offset, 8));
}


u8 jedi_state::speech_ready_r()
{
	return m_tms->readyq_r() << 7;
}


void jedi_state::speech_reset_w(u8 data)
{
	// Flip-flop at 8C controls the power supply to the TMS5220 (through transistors Q6 and Q7)
	m_tms->set_output_gain(ALL_OUTPUTS, BIT(data, 0) ? 1.0 : 0.0);
}



/*************************************
 *
 *  Audio CPU memory handlers
 *
 *************************************/

void jedi_state::audio_map(address_map &map)
{
	map(0x0000, 0x07ff).ram();
	map(0x0800, 0x080f).mirror(0x07c0).rw("pokey1", FUNC(pokey_device::read), FUNC(pokey_device::write));
	map(0x0810, 0x081f).mirror(0x07c0).rw("pokey2", FUNC(pokey_device::read), FUNC(pokey_device::write));
	map(0x0820, 0x082f).mirror(0x07c0).rw("pokey3", FUNC(pokey_device::read), FUNC(pokey_device::write));
	map(0x0830, 0x083f).mirror(0x07c0).rw("pokey4", FUNC(pokey_device::read), FUNC(pokey_device::write));
	map(0x1000, 0x1000).mirror(0x00ff).nopr().w(FUNC(jedi_state::irq_ack_w));
	map(0x1100, 0x1100).mirror(0x00ff).nopr().w(m_tms, FUNC(tms5220_device::data_w));
	map(0x1200, 0x13ff).nopr().w(FUNC(jedi_state::speech_strobe_w));
	map(0x1400, 0x1400).mirror(0x00ff).nopr().w(m_sacklatch, FUNC(generic_latch_8_device::write));
	map(0x1500, 0x1500).mirror(0x00ff).nopr().w(FUNC(jedi_state::speech_reset_w));
	map(0x1600, 0x17ff).noprw();
	map(0x1800, 0x1800).mirror(0x03ff).r(m_soundlatch, FUNC(generic_latch_8_device::read)).nopw();
	map(0x1c00, 0x1c00).mirror(0x03fe).r(FUNC(jedi_state::speech_ready_r)).nopw();
	map(0x1c01, 0x1c01).mirror(0x03fe).r(FUNC(jedi_state::audio_comm_stat_r)).nopw();
	map(0x2000, 0x7fff).noprw();
	map(0x8000, 0xffff).rom();
}

/***************************************************************************

    Return of the Jedi has a peculiar playfield/motion object
    priority system. That is, there is no priority system ;-)
    The color of the pixel which appears on screen depends on
    all three of the foreground, background and motion objects.
    The 1024 colors palette is appropriately set up by the program
    to "emulate" a priority system, but it can also be used to display
    completely different colors (see the palette test in service mode)

***************************************************************************/

/*************************************
 *
 *  Start
 *
 *************************************/

void jedi_state::video_start()
{
#if DEBUG_GFXDECODE
	// the sprite pixel determines pen address bits A4-A7
	gfx_element *gx0 = m_gfxdecode->gfx(2);

	// allocate memory for the assembled data
	m_gfxdata = std::make_unique<u8[]>(gx0->elements() * gx0->width() * gx0->height());

	// loop over elements
	u8 *dest = m_gfxdata.get();
	for (int c = 0; c < gx0->elements(); c++)
	{
		const u8 *c0base = gx0->get_data(c);

		// loop over height
		for (int y = 0; y < gx0->height(); y++)
		{
			const u8 *c0 = c0base;

			for (int x = 0; x < gx0->width(); x++)
			{
				const u8 pix = (*c0++ & 0xf);
				*dest++ = pix << 4;
			}
			c0base += gx0->rowbytes();
		}
	}

	gx0->set_raw_layout(m_gfxdata.get(), gx0->width(), gx0->height(), gx0->elements(), 8 * gx0->width(), 8 * gx0->width() * gx0->height());
	gx0->set_granularity(1);
#endif

	// register for saving
	save_item(NAME(m_vscroll));
	save_item(NAME(m_hscroll));
	save_item(NAME(m_foreground_bank));
	save_item(NAME(m_video_off));
}


void jedi_state::foreground_bank_w(int state)
{
	m_foreground_bank = state;
}


void jedi_state::video_off_w(int state)
{
	m_video_off = state;
}


/*************************************
 *
 *  Palette RAM
 *
 *************************************
 *
 *  Color RAM format
 *  Color RAM is 1024x12
 *
 *  RAM address: A0..A3 = Playfield color code
 *      A4..A7 = Motion object color code
 *      A8..A9 = Alphanumeric color code
 *
 *  RAM data:
 *      0..2 = Blue
 *      3..5 = Green
 *      6..8 = Blue
 *      9..11 = Intensity
 *
 *  Output resistor values:
 *      bit 0 = 22K
 *      bit 1 = 10K
 *      bit 2 = 4.7K
 *
 *************************************/

rgb_t jedi_state::jedi_IRGB_3333(u32 raw)
{
	const u8 intensity = (raw >> 9) & 7;
	u8 bits = (raw >> 6) & 7;
	const u8 r = 5 * bits * intensity;
	bits = (raw >> 3) & 7;
	const u8 g = 5 * bits * intensity;
	bits = (raw >> 0) & 7;
	const u8 b = 5 * bits * intensity;

	return rgb_t(r, g, b);
}

void jedi_state::do_pen_lookup(bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	for (int y = cliprect.top(); y <= cliprect.bottom(); y++)
		for(int x = cliprect.left(); x <= cliprect.right(); x++)
			bitmap.pix(y, x) = m_palette->pen(bitmap.pix(y, x));
}


/*************************************
 *
 *  Scroll offsets
 *
 *************************************/

void jedi_state::vscroll_w(offs_t offset, u8 data)
{
	m_vscroll = data | (offset << 8);
}


void jedi_state::hscroll_w(offs_t offset, u8 data)
{
	m_hscroll = data | (offset << 8);
}


/*************************************
 *
 *  Background/text layer drawing
 *  with smoothing
 *
 *************************************/

void jedi_state::draw_background_and_text(bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	u8 background_line_buffer[0x200]; // RAM chip at 2A

	const u8 *prom1 = &m_proms[0x0000 | ((*m_smoothing_table & 0x03) << 8)];
	const u8 *prom2 = &m_proms[0x0800 | ((*m_smoothing_table & 0x03) << 8)];

	std::fill(std::begin(background_line_buffer), std::end(background_line_buffer), 0);

	for (int y = cliprect.top(); y <= cliprect.bottom(); y++)
	{
		u8 bg_last_col = 0;

		for (int x = cliprect.left(); x <= cliprect.right(); x += 2)
		{
			u16 tx_col1, tx_col2, bg_col = 0;

			const int sy = y + m_vscroll;
			int sx = x + m_hscroll;

			// determine offsets into video memory
			const offs_t tx_offs = ((y & 0xf8) << 3) | (x >> 3);
			const offs_t bg_offs = ((sy & 0x1f0) << 1) | ((sx & 0x1f0) >> 4);

			// get the character codes
			const int tx_code = (m_foreground_bank << 8) | m_foregroundram[tx_offs];
			const int bg_bank = m_backgroundram[0x0400 | bg_offs];
			const int bg_code = m_backgroundram[0x0000 | bg_offs] |
					((bg_bank & 0x01) << 8) |
					((bg_bank & 0x08) << 6) |
					((bg_bank & 0x02) << 9);

			// background flip X
			if (bg_bank & 0x04)
				sx = sx ^ 0x0f;

			// calculate the address of the gfx data
			const offs_t tx_gfx_offs = (tx_code << 4) | ((y & 0x07) << 1) | ((( x & 0x04) >> 2));
			const offs_t bg_gfx_offs = (bg_code << 4) | (sy & 0x0e)       | (((sx & 0x08) >> 3));

			// get the gfx data
			const u8 tx_data  = m_tx_gfx[         tx_gfx_offs];
			const u8 bg_data1 = m_bg_gfx[0x0000 | bg_gfx_offs];
			const u8 bg_data2 = m_bg_gfx[0x8000 | bg_gfx_offs];

			// the text layer pixel determines pen address bits A8 and A9
			if (x & 0x02)
			{
				tx_col1 = ((tx_data & 0x0c) << 6);
				tx_col2 = ((tx_data & 0x03) << 8);
			}
			else
			{
				tx_col1 = ((tx_data & 0xc0) << 2);
				tx_col2 = ((tx_data & 0x30) << 4);
			}

			// the background pixel determines pen address bits A0-A3
			switch (sx & 0x06)
			{
			case 0x00: bg_col = ((bg_data1 & 0x80) >> 4) | ((bg_data1 & 0x08) >> 1) | ((bg_data2 & 0x80) >> 6) | ((bg_data2 & 0x08) >> 3); break;
			case 0x02: bg_col = ((bg_data1 & 0x40) >> 3) | ((bg_data1 & 0x04) >> 0) | ((bg_data2 & 0x40) >> 5) | ((bg_data2 & 0x04) >> 2); break;
			case 0x04: bg_col = ((bg_data1 & 0x20) >> 2) | ((bg_data1 & 0x02) << 1) | ((bg_data2 & 0x20) >> 4) | ((bg_data2 & 0x02) >> 1); break;
			case 0x06: bg_col = ((bg_data1 & 0x10) >> 1) | ((bg_data1 & 0x01) << 2) | ((bg_data2 & 0x10) >> 3) | ((bg_data2 & 0x01) >> 0); break;
			}

			/* the first pixel is smoothed via a lookup using the current and last pixel value -
			   the next pixel just uses the current value directly. After we done with a pixel
			   save it for later in the line buffer RAM */
			const u8 bg_tempcol = prom1[(bg_last_col << 4) | bg_col];
			bitmap.pix(y, x + 0) = tx_col1 | prom2[(background_line_buffer[x + 0] << 4) | bg_tempcol];
			bitmap.pix(y, x + 1) = tx_col2 | prom2[(background_line_buffer[x + 1] << 4) | bg_col];
			background_line_buffer[x + 0] = bg_tempcol;
			background_line_buffer[x + 1] = bg_col;

			bg_last_col = bg_col;
		}
	}
}


/*************************************
 *
 *  Sprite drawing
 *
 *************************************/

void jedi_state::draw_sprites(bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	for (offs_t offs = 0x00; offs < 0x30; offs++)
	{
		int y_size;

		// coordinates adjustments made to match screenshot
		u8 y = 240 - m_spriteram[offs + 0x80] + 1;
		const bool flip_x = m_spriteram[offs + 0x40] & 0x10;
		const bool flip_y = m_spriteram[offs + 0x40] & 0x20;
		const bool tall = m_spriteram[offs + 0x40] & 0x08;

		// shuffle the bank bits in
		u16 code = m_spriteram[offs] |
				((m_spriteram[offs + 0x40] & 0x04) << 8) |
				((m_spriteram[offs + 0x40] & 0x40) << 3) |
				((m_spriteram[offs + 0x40] & 0x02) << 7);

		// adjust for double-height
		if (tall)
		{
			code &= ~1;
			y_size = 0x20;
			y = y - 0x10;
		}
		else
			y_size = 0x10;

		const u8 *gfx = &m_spr_gfx[code << 5];

		if (flip_y)
			y = y + y_size - 1;

		for (int sy = 0; sy < y_size; sy++)
		{
			u16 x = m_spriteram[offs + 0x100] + ((m_spriteram[offs + 0x40] & 0x01) << 8) - 2;

			if (flip_x)
				x = x + 7;

			for (int i = 0; i < 2; i++)
			{
				u8 data1 = *(0x00000 + gfx);
				u8 data2 = *(0x10000 + gfx);

				for (int sx = 0; sx < 4; sx++)
				{
					// the sprite pixel determines pen address bits A4-A7
					const u32 col = ((data1 & 0x80) >> 0) | ((data1 & 0x08) << 3) | ((data2 & 0x80) >> 2) | ((data2 & 0x08) << 1);

					x = x & 0x1ff;

					if (col && cliprect.contains(x, y))
						bitmap.pix(y, x) = (bitmap.pix(y, x) & 0x30f) | col;

					// next pixel
					if (flip_x)
						x = x - 1;
					else
						x = x + 1;

					data1 = data1 << 1;
					data2 = data2 << 1;
				}

				gfx = gfx + 1;
			}

			if (flip_y)
				y = y - 1;
			else
				y = y + 1;
		}
	}
}


/*************************************
 *
 *  Core video refresh
 *
 *************************************/

u32 jedi_state::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	// if no video, clear it all to black
	if (m_video_off)
		bitmap.fill(rgb_t::black(), cliprect);
	else
	{
		// draw the background/text layers, followed by the sprites - it needs to be done in this order
		draw_background_and_text(bitmap, cliprect);
		draw_sprites(bitmap, cliprect);
		do_pen_lookup(bitmap, cliprect);
	}

	return 0;
}


/*************************************
 *
 *  Machine driver
 *
 *************************************/

#if DEBUG_GFXDECODE
static const gfx_layout text_layout =
{
	8, 8,
	RGN_FRAC(1,1),
	2,
	{ 0, 1 },
	{ STEP8(0, 2) },
	{ STEP8(0, 2*8) },
	8*8*2
};

static const gfx_layout bg_layout =
{
	8, 8,
	RGN_FRAC(1,2),
	4,
	{ 0, 4, RGN_FRAC(1,2)+0, RGN_FRAC(1,2)+4 },
	{ STEP4(0, 1), STEP4(4*2, 1) },
	{ STEP8(0, 4*2*2) },
	8*8*2
};

static const gfx_layout sprite_layout =
{
	8, 16,
	RGN_FRAC(1,2),
	4,
	{ 0, 4, RGN_FRAC(1,2)+0, RGN_FRAC(1,2)+4 },
	{ STEP4(0, 1), STEP4(4*2, 1) },
	{ STEP16(0, 4*2*2) },
	8*16*2
};

static GFXDECODE_START( gfx_jedi )
	GFXDECODE_ENTRY( "tx_gfx",  0, text_layout,   0, 0x400/0x04 )
	GFXDECODE_SCALE( "bg_gfx",  0, bg_layout,     0, 0x400/0x10, 2, 2 ) // 8x8 but internally expanded related with smoothing
	GFXDECODE_ENTRY( "spr_gfx", 0, sprite_layout, 0, 0x310 )
GFXDECODE_END
#endif


/*************************************
 *
 *  Interrupt handling
 *
 *************************************/

TIMER_CALLBACK_MEMBER(jedi_state::generate_interrupt)
{
	int scanline = param;

	// IRQ is set by /32V
	m_maincpu->set_input_line(M6502_IRQ_LINE, (scanline & 32) ? CLEAR_LINE : ASSERT_LINE);
	m_audiocpu->set_input_line(M6502_IRQ_LINE, (scanline & 32) ? CLEAR_LINE : ASSERT_LINE);

	// set up for the next
	scanline += 32;
	if (scanline > 256)
		scanline = 32;
	m_interrupt_timer->adjust(m_screen->time_until_pos(scanline), scanline);
}


void jedi_state::main_irq_ack_w(u8 data)
{
	m_maincpu->set_input_line(M6502_IRQ_LINE, CLEAR_LINE);
}


/*************************************
 *
 *  Start
 *
 *************************************/

void jedi_state::machine_start()
{
	// set a timer to run the interrupts
	m_interrupt_timer = timer_alloc(FUNC(jedi_state::generate_interrupt), this);
	m_interrupt_timer->adjust(m_screen->time_until_pos(32), 32);

	// configure the banks
	m_mainbank->configure_entries(0, 3, memregion("maincpu")->base() + 0x10000, 0x4000);
}


/*************************************
 *
 *  Reset
 *
 *************************************/

void jedi_state::machine_reset()
{
	m_novram[0]->recall(1);
	m_novram[1]->recall(1);
}


/*************************************
 *
 *  Main program ROM banking
 *
 *************************************/

void jedi_state::rom_banksel_w(u8 data)
{
	if (data & 0x01) m_mainbank->set_entry(0);
	if (data & 0x02) m_mainbank->set_entry(1);
	if (data & 0x04) m_mainbank->set_entry(2);
}


/*************************************
 *
 *  I/O ports
 *
 *************************************/

 template <uint8_t Which> // 0 left, 1 right
void jedi_state::coin_counter_w(int state)
{
	machine().bookkeeping().coin_counter_w(Which, state);
}


/*************************************
 *
 *  X2212-30 NOVRAM
 *
 *************************************/

u8 jedi_state::novram_data_r(address_space &space, offs_t offset)
{
	return (m_novram[0]->read(space, offset) & 0x0f) | (m_novram[1]->read(space, offset) << 4);
}


void jedi_state::novram_data_w(offs_t offset, u8 data)
{
	m_novram[0]->write(offset, data & 0x0f);
	m_novram[1]->write(offset, data >> 4);
}


void jedi_state::novram_recall_w(offs_t offset, u8 data)
{
	m_novram[0]->recall(BIT(offset, 0));
	m_novram[1]->recall(BIT(offset, 0));
}


void jedi_state::novram_store_w(u8 data)
{
	m_novram[0]->store(1);
	m_novram[1]->store(1);
	m_novram[0]->store(0);
	m_novram[1]->store(0);
}


/*************************************
 *
 *  Main CPU memory handlers
 *
 *************************************/

void jedi_state::main_map(address_map &map)
{
	map(0x0000, 0x07ff).ram();
	map(0x0800, 0x08ff).mirror(0x0300).rw(FUNC(jedi_state::novram_data_r), FUNC(jedi_state::novram_data_w));
	map(0x0c00, 0x0c00).mirror(0x03fe).portr("0c00").nopw();
	map(0x0c01, 0x0c01).mirror(0x03fe).portr("0c01").nopw();
	map(0x1000, 0x13ff).noprw();
	map(0x1400, 0x1400).mirror(0x03ff).r("sacklatch", FUNC(generic_latch_8_device::read)).nopw();
	map(0x1800, 0x1800).mirror(0x03ff).r("adc", FUNC(adc0808_device::data_r)).nopw();
	map(0x1c00, 0x1c01).mirror(0x007e).nopr().w(FUNC(jedi_state::novram_recall_w));
	map(0x1c80, 0x1c87).mirror(0x0078).nopr().w("adc", FUNC(adc0808_device::address_offset_start_w));
	map(0x1d00, 0x1d00).mirror(0x007f).nopr().w(FUNC(jedi_state::novram_store_w));
	map(0x1d80, 0x1d80).mirror(0x007f).nopr().w("watchdog", FUNC(watchdog_timer_device::reset_w));
	map(0x1e00, 0x1e00).mirror(0x007f).nopr().w(FUNC(jedi_state::main_irq_ack_w));
	map(0x1e80, 0x1e87).mirror(0x0078).nopr().w("outlatch", FUNC(ls259_device::write_d7));
	map(0x1f00, 0x1f00).mirror(0x007f).nopr().w(m_soundlatch, FUNC(generic_latch_8_device::write));
	map(0x1f80, 0x1f80).mirror(0x007f).nopr().w(FUNC(jedi_state::rom_banksel_w));
	map(0x2000, 0x27ff).ram().share(m_backgroundram);
	map(0x2800, 0x2bff).ram().w(m_palette, FUNC(palette_device::write8)).share("palette");
	map(0x2c00, 0x2fff).ram().w(m_palette, FUNC(palette_device::write8_ext)).share("palette_ext");
	map(0x3000, 0x37bf).ram().share(m_foregroundram);
	map(0x37c0, 0x3bff).ram().share(m_spriteram);
	map(0x3c00, 0x3c01).mirror(0x00fe).nopr().w(FUNC(jedi_state::vscroll_w));
	map(0x3d00, 0x3d01).mirror(0x00fe).nopr().w(FUNC(jedi_state::hscroll_w));
	map(0x3e00, 0x3e00).mirror(0x01ff).writeonly().share("smoothing_table");
	map(0x4000, 0x7fff).bankr(m_mainbank);
	map(0x8000, 0xffff).rom();
}


/*************************************
 *
 *  Port definitions
 *
 *************************************/

static INPUT_PORTS_START( jedi )
	PORT_START("0c00")
	PORT_BIT( 0x01, IP_ACTIVE_LOW,  IPT_BUTTON3 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW,  IPT_BUTTON2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW,  IPT_BUTTON1 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )
	PORT_BIT( 0x20, IP_ACTIVE_LOW,  IPT_SERVICE1 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW,  IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW,  IPT_COIN1 )

	PORT_START("0c01")
	PORT_BIT( 0x03, IP_ACTIVE_LOW,  IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_TILT )
	PORT_BIT( 0x18, IP_ACTIVE_LOW,  IPT_UNUSED )
	PORT_BIT( 0x60, IP_ACTIVE_HIGH, IPT_CUSTOM ) PORT_CUSTOM_MEMBER(FUNC(jedi_state::audio_comm_stat_0c01_r))
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_CUSTOM ) PORT_READ_LINE_DEVICE_MEMBER("screen", FUNC(screen_device::vblank))

	PORT_START("STICKY")
	PORT_BIT( 0xff, 0x80, IPT_AD_STICK_Y ) PORT_SENSITIVITY(100) PORT_KEYDELTA(10)

	PORT_START("STICKX")
	PORT_BIT( 0xff, 0x80, IPT_AD_STICK_X ) PORT_SENSITIVITY(100) PORT_KEYDELTA(10)
INPUT_PORTS_END


/*************************************
 *
 *  Machine driver
 *
 *************************************/

void jedi_state::jedi(machine_config &config)
{
	constexpr XTAL JEDI_MAIN_CPU_OSC = XTAL(10'000'000);
	constexpr XTAL JEDI_AUDIO_CPU_OSC = XTAL(12'096'000);
	constexpr XTAL JEDI_MAIN_CPU_CLOCK = JEDI_MAIN_CPU_OSC / 4;
	constexpr XTAL JEDI_AUDIO_CPU_CLOCK = JEDI_AUDIO_CPU_OSC / 8;
	constexpr XTAL JEDI_POKEY_CLOCK = JEDI_AUDIO_CPU_CLOCK;
	constexpr XTAL JEDI_TMS5220_CLOCK = JEDI_AUDIO_CPU_OSC / 2 / 9; // div by 9 is via a binary counter that counts from 7 to 16

	// basic machine hardware
	M6502(config, m_maincpu, JEDI_MAIN_CPU_CLOCK);
	m_maincpu->set_addrmap(AS_PROGRAM, &jedi_state::main_map);

	config.set_maximum_quantum(attotime::from_hz(240));

	X2212(config, "novram12b");
	X2212(config, "novram12c");

	adc0809_device &adc(ADC0809(config, "adc", JEDI_AUDIO_CPU_OSC / 2 / 9)); // nominally 666 kHz
	adc.in_callback<0>().set_ioport("STICKY");
	adc.in_callback<1>().set_constant(0); // SPARE
	adc.in_callback<2>().set_ioport("STICKX");
	adc.in_callback<3>().set_constant(0); // SPARE

	ls259_device &outlatch(LS259(config, "outlatch")); // 14J
	outlatch.q_out_cb<0>().set(FUNC(jedi_state::coin_counter_w<0>));
	outlatch.q_out_cb<1>().set(FUNC(jedi_state::coin_counter_w<1>));
	outlatch.q_out_cb<2>().set_nop(); // LED control - not used
	outlatch.q_out_cb<3>().set_nop(); // LED control - not used
	outlatch.q_out_cb<4>().set(FUNC(jedi_state::foreground_bank_w));
	outlatch.q_out_cb<6>().set(FUNC(jedi_state::audio_reset_w));
	outlatch.q_out_cb<7>().set(FUNC(jedi_state::video_off_w));

	WATCHDOG_TIMER(config, "watchdog");

	// video hardware
#if DEBUG_GFXDECODE
	GFXDECODE(config, m_gfxdecode, m_palette, gfx_jedi);
#endif
	SCREEN(config, m_screen, SCREEN_TYPE_RASTER);
	m_screen->set_refresh_hz(60);
	m_screen->set_size(64*8, 262); // verify vert size
	m_screen->set_visarea(0*8, 37*8-1, 0*8, 30*8-1);
	m_screen->set_screen_update(FUNC(jedi_state::screen_update));

	PALETTE(config, m_palette).set_format(2, &jedi_state::jedi_IRGB_3333, 0x400);

	// audio hardware
	M6502(config, m_audiocpu, JEDI_AUDIO_CPU_CLOCK);
	m_audiocpu->set_addrmap(AS_PROGRAM, &jedi_state::audio_map);

	SPEAKER(config, "speaker", 2).front();

	pokey_device &pokey1(POKEY(config, "pokey1", JEDI_POKEY_CLOCK));
	pokey1.set_output_opamp(RES_K(1), 0.0, 5.0);
	pokey1.add_route(ALL_OUTPUTS, "speaker", 0.30, 0);
	pokey1.add_route(ALL_OUTPUTS, "speaker", 0.30, 1);

	pokey_device &pokey2(POKEY(config, "pokey2", JEDI_POKEY_CLOCK));
	pokey2.set_output_opamp(RES_K(1), 0.0, 5.0);
	pokey2.add_route(ALL_OUTPUTS, "speaker", 0.30, 0);
	pokey2.add_route(ALL_OUTPUTS, "speaker", 0.30, 1);

	pokey_device &pokey3(POKEY(config, "pokey3", JEDI_POKEY_CLOCK));
	pokey3.set_output_opamp(RES_K(1), 0.0, 5.0);
	pokey3.add_route(ALL_OUTPUTS, "speaker", 0.30, 0);

	pokey_device &pokey4(POKEY(config, "pokey4", JEDI_POKEY_CLOCK));
	pokey4.set_output_opamp(RES_K(1), 0.0, 5.0);
	pokey4.add_route(ALL_OUTPUTS, "speaker", 0.30, 1);

	TMS5220(config, m_tms, JEDI_TMS5220_CLOCK);
	m_tms->add_route(ALL_OUTPUTS, "speaker", 1.0, 0);
	m_tms->add_route(ALL_OUTPUTS, "speaker", 1.0, 1);

	GENERIC_LATCH_8(config, m_soundlatch); // 5E (LS374) + 3E (LS279) pins 13-15
	GENERIC_LATCH_8(config, m_sacklatch); // 4E (LS374) + 3E (LS279) pins 1-4
}


/*************************************
 *
 *  ROM definitions
 *
 *************************************/

ROM_START( jedi )
	ROM_REGION( 0x1C000, "maincpu", 0 )
	ROM_LOAD( "136030-221.14f",  0x08000, 0x4000, CRC(414d05e3) SHA1(e5f5f8d85433467a13d6ca9e3889e07a62b00e52) )
	ROM_LOAD( "136030-222.13f",  0x0c000, 0x4000, CRC(7b3f21be) SHA1(8fe62401f9b78c7a3e62b544c4b705b1bfa9b8f3) )
	ROM_LOAD( "136030-123.13d",  0x10000, 0x4000, CRC(877f554a) SHA1(8b51109cabd84741b024052f892b3172fbe83223) ) // Page 0
	ROM_LOAD( "136030-124.13b",  0x14000, 0x4000, CRC(e72d41db) SHA1(1b3fcdc435f1e470e8d5b7241856e398a4c3910e) ) // Page 1
	ROM_LOAD( "136030-122.13a",  0x18000, 0x4000, CRC(cce7ced5) SHA1(bff031a637aefca713355dbf251dcb5c2cea0885) ) // Page 2

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "136030-133.01c",  0x8000, 0x4000, CRC(6c601c69) SHA1(618b77800bbbb4db34a53ca974a71bdaf89b5930) )
	ROM_LOAD( "136030-134.01a",  0xC000, 0x4000, CRC(5e36c564) SHA1(4b0afceb9a1d912f1d5c1f26928d244d5b14ea4a) )

	ROM_REGION( 0x02000, "tx_gfx",0 )
	ROM_LOAD( "136030-215.11t",  0x00000, 0x2000, CRC(3e49491f) SHA1(ade5e846069c2fa6edf667469d13ce5a6a45c06d) )

	ROM_REGION( 0x10000, "bg_gfx",0 )
	ROM_LOAD( "136030-126.06r",  0x00000, 0x8000, CRC(9c55ece8) SHA1(b8faa23314bb0d199ef46199bfabd9cb17510dd3) )
	ROM_LOAD( "136030-127.06n",  0x08000, 0x8000, CRC(4b09dcc5) SHA1(d46b5f4fb69c4b8d823dd9c4d92f8713badfa44a) )

	ROM_REGION( 0x20000, "spr_gfx", 0 )
	ROM_LOAD( "136030-130.01h",  0x00000, 0x8000, CRC(2646a793) SHA1(dcb5fd50eafbb27565bce099a884be83a9d82285) )
	ROM_LOAD( "136030-131.01f",  0x08000, 0x8000, CRC(60107350) SHA1(ded03a46996d3f2349df7f59fd435a7ad6ed465e) )
	ROM_LOAD( "136030-128.01m",  0x10000, 0x8000, CRC(24663184) SHA1(5eba142ed926671ee131430944e59f21a55a5c57) )
	ROM_LOAD( "136030-129.01k",  0x18000, 0x8000, CRC(ac86b98c) SHA1(9f86c8801a7293fa46e9432f1651dd85bf00f4b9) )

	ROM_REGION( 0x1000, "proms", 0 )    // background smoothing
	ROM_LOAD( "136030-117.bin",   0x0000, 0x0400, CRC(9831bd55) SHA1(12945ef2d1582914125b9ee591567034d71d6573) )
	ROM_LOAD( "136030-118.bin",   0x0800, 0x0400, CRC(261fbfe7) SHA1(efc65a74a3718563a07b718e34d8a7aa23339a69) )

	// NOVRAM default fills are specified to guarantee an invalid checksum
	ROM_REGION( 0x100, "novram12b", 0 )
	ROM_FILL( 0x00, 0x100, 0x0f )
	ROM_FILL( 0x50, 0x010, 0x00 )
	ROM_REGION( 0x100, "novram12c", 0 )
	ROM_FILL( 0x00, 0x100, 0x0f )
	ROM_FILL( 0x50, 0x010, 0x00 )
ROM_END

} // anonymous namespace


/*************************************
 *
 *  Game drivers
 *
 *************************************/

GAME( 1984, jedi, 0, jedi, jedi, jedi_state, empty_init, ROT0, "Atari", "Return of the Jedi", MACHINE_SUPPORTS_SAVE )
