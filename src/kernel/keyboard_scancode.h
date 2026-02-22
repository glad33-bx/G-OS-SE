#ifndef SCANCODE_H
#define SCANCODE_H

#define ESCAPE 0x01    /* ESCAPE */
#define KEY_1 0x02     /* KEY_1 */
#define KEY_2 0x03     /* KEY_2 */
#define KEY_3 0x04     /* KEY_3 */
#define KEY_4 0x05     /* KEY_4 */
#define KEY_5 0x06     /* KEY_5 */
#define KEY_6 0x07     /* KEY_6 */
#define KEY_7 0x08     /* KEY_7 */
#define KEY_8 0x09     /* KEY_8 */
#define KEY_9 0x0A     /* KEY_9 */
#define KEY_0 0x0B     /* KEY_0 */
#define OEM_MINUS 0x0C /* OEM_MINUS */
#define OEM_PLUS 0x0D  /* OEM_PLUS */
#define BACKSPACE 0x0E /* BACK Backspace */
#define TAB 0x0F       /* TAB */
#define KEY_Q 0x10     /* KEY_Q */
#define KEY_W 0x11     /* KEY_W */
#define KEY_E 0x12     /* KEY_E */
#define KEY_R 0x13     /* KEY_R */
#define KEY_T 0x14     /* KEY_T */
#define KEY_Y 0x15     /* KEY_Y */
#define KEY_U 0x16     /* KEY_U */
#define KEY_I 0x17     /* KEY_I */
#define KEY_O 0x18     /* KEY_O */
#define KEY_P 0x19     /* KEY_P */
#define OEM_4 0x1A     /* OEM_4 '[' on US */
#define OEM_6 0x1B     /* OEM_6 ']' on US */
#define RETURN 0x1C    /* RETURN Normal Enter */
#define LCONTROL 0x1D  /* LCONTROL */
#define KEY_A 0x1E     /* KEY_A */
#define KEY_S 0x1F     /* KEY_S */
#define KEY_D 0x20     /* KEY_D */
#define KEY_F 0x21     /* KEY_F */
#define KEY_G 0x22     /* KEY_G */
#define KEY_H 0x23     /* KEY_H */
#define KEY_J 0x24     /* KEY_J */
#define KEY_K 0x25     /* KEY_K */
#define KEY_L 0x26     /* KEY_L */
#define OEM_1 0x27     /* OEM_1 ';' on US */
#define OEM_7 0x28     /* OEM_7 "'" on US */
#define OEM_3 \
	0x29 /* OEM_3 Top left, '`' on US, JP DBE_SBCSCHAR */
#define LSHIFT 0x2A /* LSHIFT */
#define OEM_5 0x2B  /* OEM_5 Next to Enter, '\' on US */
#define KEY_Z 0x2C  /* KEY_Z */
#define KEY_X 0x2D  /* KEY_X */
#define KEY_C 0x2E  /* KEY_C */
#define KEY_V 0x2F  /* KEY_V */
#define KEY_B 0x30  /* KEY_B */
#define KEY_N 0x31  /* KEY_N */
#define KEY_M 0x32  /* KEY_M */
#define OEM_COMMA 0x33  /* OEM_COMMA */
#define OEM_PERIOD 0x34 /* OEM_PERIOD */
#define OEM_2 0x35      /* OEM_2 '/' on US */
#define RSHIFT 0x36     /* RSHIFT */
#define MULTIPLY 0x37   /* MULTIPLY Numerical */
#define LMENU 0x38      /* LMENU Left 'Alt' key */
#define SPACE 0x39      /* SPACE */
#define CAPSLOCK \
	0x3A /* CAPITAL 'Caps Lock', JP DBE_ALPHANUMERIC */
#define F1 0x3B  /* F1 */
#define F2 0x3C  /* F2 */
#define F3 0x3D  /* F3 */
#define F4 0x3E  /* F4 */
#define F5 0x3F  /* F5 */
#define F6 0x40  /* F6 */
#define F7 0x41  /* F7 */
#define F8 0x42  /* F8 */
#define F9 0x43  /* F9 */
#define F10 0x44 /* F10 */
#define NUMLOCK                                                                     \
	0x45                                                               \
	/* NUMLOCK */ /* Note: when this seems to appear in PKBDLLHOOKSTRUCT it means Pause which \
	                    must be sent as Ctrl + NumLock */
#define SCROLLLOCK \
	0x46 /* SCROLL 'Scroll Lock', JP OEM_SCROLL */
#define NUMPAD7 0x47  /* NUMPAD7 */
#define NUMPAD8 0x48  /* NUMPAD8 */
#define NUMPAD9 0x49  /* NUMPAD9 */
#define SUBTRACT 0x4A /* SUBTRACT */
#define NUMPAD4 0x4B  /* NUMPAD4 */
#define NUMPAD5 0x4C  /* NUMPAD5 */
#define NUMPAD6 0x4D  /* NUMPAD6 */
#define ADD 0x4E      /* ADD */
#define NUMPAD1 0x4F  /* NUMPAD1 */
#define NUMPAD2 0x50  /* NUMPAD2 */
#define NUMPAD3 0x51  /* NUMPAD3 */
#define NUMPAD0 0x52  /* NUMPAD0 */
#define DECIMAL 0x53  /* DECIMAL Numerical, '.' on US */
#define SYSREQ 0x54   /* Sys Req */
#define OEM_102 0x56  /* OEM_102 Lower left '\' on US */
#define F11 0x57      /* F11 */
#define F12 0x58      /* F12 */
#define SLEEP                                                                       \
	0x5F                       /* SLEEP OEM_8 on FR (undocumented?) \
	                                                      */
#define ZOOM 0x62 /* ZOOM (undocumented?) */
#define HELP 0x63 /* HELP (undocumented?) */

#define F13 \
	0x64 /* F13 */ /* JP agree, should 0x7d according to ms894073 */
#define F14 0x65              /* F14 */
#define F15 0x66              /* F15 */
#define F16 0x67              /* F16 */
#define F17 0x68              /* F17 */
#define F18 0x69              /* F18 */
#define F19 0x6A              /* F19 */
#define F20 0x6B              /* F20 */
#define F21 0x6C              /* F21 */
#define F22 0x6D              /* F22 */
#define F23 0x6E /* F23 */ /* JP agree */
#define F24 \
	0x6F /* F24 */ /* 0x87 according to ms894073 */

#define HIRAGANA 0x70 /* JP DBE_HIRAGANA */
#define HANJA_KANJI \
	0x71 /* HANJA / KANJI (undocumented?) */
#define KANA_HANGUL \
	0x72 /* KANA / HANGUL (undocumented?) */
#define ABNT_C1 0x73       /* ABNT_C1 JP OEM_102 */
#define F24_JP 0x76        /* JP F24 */
#define CONVERT_JP 0x79    /* JP CONVERT */
#define NONCONVERT_JP 0x7B /* JP NONCONVERT */
#define TAB_JP 0x7C        /* JP TAB */
#define BACKSLASH_JP 0x7D  /* JP OEM_5 ('\') */
#define ABNT_C2 0x7E       /* ABNT_C2, JP */
#define HANJA 0x71         /* KR HANJA */
#define HANGUL 0x72        /* KR HANGUL */

#define RETURN_KP \
	0x1C, TRUE) /* not RETURN Numerical Enter */
#define RCONTROL 0x1D, TRUE) /* RCONTROL */
#define DIVIDE 0x35, TRUE)   /* DIVIDE Numerical */
#define PRINTSCREEN \
	0x37, TRUE) /* EXECUTE/PRINT/SNAPSHOT Print Screen */
#define RMENU 0x38, TRUE) /* RMENU Right 'Alt' / 'Alt Gr' */
#define PAUSE \
	0x46, TRUE) /* PAUSE Pause / Break (Slightly special handling) */
#define HOME 0x47, TRUE)   /* HOME */
#define UP 0x48, TRUE)     /* UP */
#define PRIOR 0x49, TRUE)  /* PRIOR 'Page Up' */
#define LEFT 0x4B, TRUE)   /* LEFT */
#define RIGHT 0x4D, TRUE)  /* RIGHT */
#define END 0x4F, TRUE)    /* END */
#define DOWN 0x50, TRUE)   /* DOWN */
#define NEXT 0x51, TRUE)   /* NEXT 'Page Down' */
#define INSERT 0x52, TRUE) /* INSERT */
#define DELETE 0x53, TRUE) /* DELETE */
#define NULL 0x54, TRUE)   /* <00> */
#define HELP2 \
	0x56, TRUE) /* Help - documented, different from HELP */
#define LWIN 0x5B, TRUE)     /* LWIN */
#define RWIN 0x5C, TRUE)     /* RWIN */
#define APPS 0x5D, TRUE)     /* APPS Application */
#define POWER_JP 0x5E, TRUE) /* JP POWER */
#define SLEEP_JP 0x5F, TRUE) /* JP SLEEP */

/* _not_ valid scancode, but this is what a windows PKBDLLHOOKSTRUCT for NumLock contains */
#define NUMLOCK_EXTENDED \
	0x45, TRUE) /* should be NUMLOCK */
#define RSHIFT_EXTENDED \
	0x36, TRUE) /* should be RSHIFT */

/* Audio */
#define VOLUME_MUTE 0x20, TRUE) /* VOLUME_MUTE */
#define VOLUME_DOWN 0x2E, TRUE) /* VOLUME_DOWN */
#define VOLUME_UP 0x30, TRUE)   /* VOLUME_UP */

/* Media */
#define MEDIA_NEXT_TRACK 0x19, TRUE) /* MEDIA_NEXT_TRACK */
#define MEDIA_PREV_TRACK 0x10, TRUE) /* MEDIA_PREV_TRACK */
#define MEDIA_STOP 0x24, TRUE)       /* MEDIA_MEDIA_STOP */
#define MEDIA_PLAY_PAUSE                          \
	0x22, TRUE) /* MEDIA_MEDIA_PLAY_PAUSE \
	                               */

/* Browser functions */
#define BROWSER_BACK 0x6A, TRUE)      /* BROWSER_BACK */
#define BROWSER_FORWARD 0x69, TRUE)   /* BROWSER_FORWARD */
#define BROWSER_REFRESH 0x67, TRUE)   /* BROWSER_REFRESH */
#define BROWSER_STOP 0x68, TRUE)      /* BROWSER_STOP */
#define BROWSER_SEARCH 0x65, TRUE)    /* BROWSER_SEARCH */
#define BROWSER_FAVORITES 0x66, TRUE) /* BROWSER_FAVORITES */
#define BROWSER_HOME 0x32, TRUE)      /* BROWSER_HOME */

/* Misc. */
#define LAUNCH_MAIL 0x6C, TRUE) /* LAUNCH_MAIL */

#define LAUNCH_MEDIA_SELECT                                                 \
	0x6D, TRUE)                              /* LAUNCH_MEDIA_SELECT \
	                                                            */
#define LAUNCH_APP1 0x6E, TRUE) /* LAUNCH_APP1 */
#define LAUNCH_APP2 0x6F, TRUE) /* LAUNCH_APP2 */

#endif /* FREERDP_LOCALE_KEYBOARD_H */
