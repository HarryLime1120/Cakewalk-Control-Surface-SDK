#include "stdafx.h"

#include "FilterLocator.h"
#include "MixParam.h"
#include "KeyBinding.h"

#include "MackieControl.h"
#include "MackieControlInformation.h"
#include "MackieControlState.h"
#include "MackieControlBase.h"

#include "MackieControlLCDDisplay.h"
#include "MackieControlVPotDisplay.h"
#include "MackieControlFader.h"
#include "MackieControlXT.h"

#include "MackieControl7SegmentDisplay.h"
#include "MackieControlMaster.h"
#include "MackieControlMasterPropPage.h"

#include "VirtualKeys.h"

/////////////////////////////////////////////////////////////////////////////

#define ONE_OVER_127		0.037037037037037035f
#define ONE_OVER_1023		0.0009775171065493646f

#define FKEY_MASK			(M1_TO_M4_MASK | MCS_MODIFIER_NUMERIC)
#define BANK_CHANNEL_MASK	(M1_TO_M4_MASK | MCS_MODIFIER_EDIT)
#define EDIT_MASK			(M1_TO_M4_MASK | MCS_MODIFIER_EDIT | MCS_MODIFIER_NUMERIC)
#define ZOOM_MASK			(M1_TO_M4_MASK | MCS_MODIFIER_ZOOM)

// This may only work on 2K/XP...
#define VK_OEM_COMMA		0xBC
#define VK_OEM_MINUS		0xBD

/////////////////////////////////////////////////////////////////////////////

bool CMackieControlMaster::OnFader(BYTE bD1, BYTE bD2)
{
	WORD wVal = (((WORD)bD2 << 7) | (WORD)bD1) >> 4;

	float fVal = (float)wVal * ONE_OVER_1023;

	m_SwMasterFader.SetNormalizedVal(fVal, MIX_TOUCH_MANUAL);

	return true;
}

/////////////////////////////////////////////////////////////////////////////

bool CMackieControlMaster::OnExternalController(BYTE bD2)
{
	float fVal = (float)bD2 * ONE_OVER_127;

	m_SwMasterFader.SetNormalizedVal(fVal);

	return true;
}

/////////////////////////////////////////////////////////////////////////////

bool CMackieControlMaster::OnJog(BYTE bD2)
{
	CCriticalSectionAuto csa(m_cState.GetCS());

	Direction eDir = (bD2 & 0x40) ? DIR_BACKWARD : DIR_FORWARD;

	if (m_cState.GetJogParamMode())
	{
		switch (m_cState.GetModifiers(M1_TO_M4_MASK))
		{
			case MCS_MODIFIER_NONE:						// Edit current selection
				if (DIR_FORWARD == eDir)
					FakeKeyPress(false, false, false, VK_ADD);
				else
					FakeKeyPress(false, false, false, VK_SUBTRACT);
				break;

			default:
				break;
		}
	}
	else
	{
		switch (m_cState.GetModifiers(M1_TO_M4_MASK))
		{
			case MCS_MODIFIER_NONE:						// Jog in current resolution
				ClearTransportCallbackTimer();
				NudgeTimeCursor(m_cState.GetJogResolution(), eDir);
				break;

			case MCS_MODIFIER_M1:						// Jog in measures
				ClearTransportCallbackTimer();
				NudgeTimeCursor(m_cState.GetDisplaySMPTE() ? JOG_MINUTES : JOG_MEASURES, eDir);
				break;

			case MCS_MODIFIER_M2:						// Jog in beats
				ClearTransportCallbackTimer();
				NudgeTimeCursor(m_cState.GetDisplaySMPTE() ? JOG_SECONDS : JOG_BEATS, eDir);
				break;

			case MCS_MODIFIER_M3:						// Jog in ticks
				ClearTransportCallbackTimer();
				NudgeTimeCursor(m_cState.GetDisplaySMPTE() ? JOG_FRAMES : JOG_TICKS, eDir);
				break;

			case MCS_MODIFIER_M4:						// Balance of current VMain
				{
					float fStepSize = m_SwMasterFaderPan.GetStepSize();

					HRESULT hr = m_SwMasterFaderPan.Adjust(DIR_FORWARD == eDir ? fStepSize : -fStepSize);

					// The SetVal() always reports a failure, even though it works, so
					// we can't do "if (SUCCEEDED(hr))" here

					TempDisplayMasterFaderPan();
				}
				break;

			default:
				break;
		}
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////

bool CMackieControlMaster::OnSwitch(BYTE bD1, BYTE bD2)
{
	CCriticalSectionAuto csa(m_cState.GetCS());

	bool bDown = (bD2 == 0x7F);
	
	SONAR_MIXER_STRIP eInitialMixerStrip = m_cState.GetMixerStrip();

	switch (bD1)
	{
		// -----------------------------------------------------------------------------------------------
		// When in MIX_STRAP_RACK mode, do not call OnSelectAssignment for anything other than MC_PLUG_INS
		//
		case MC_PARAM:			if (bDown && eInitialMixerStrip != MIX_STRIP_RACK) OnSelectAssignment(MCS_ASSIGNMENT_PARAMETER); break;
		case MC_SEND:			if (bDown && eInitialMixerStrip != MIX_STRIP_RACK) OnSelectAssignment(MCS_ASSIGNMENT_SEND);		 break;
		case MC_PAN:			if (bDown && eInitialMixerStrip != MIX_STRIP_RACK) OnSelectAssignment(MCS_ASSIGNMENT_PAN);		 break;
		case MC_EQ:				if (bDown && eInitialMixerStrip != MIX_STRIP_RACK) OnSelectAssignment(MCS_ASSIGNMENT_EQ);		 break;
		case MC_DYNAMICS:		if (bDown && eInitialMixerStrip != MIX_STRIP_RACK) OnSelectAssignment(MCS_ASSIGNMENT_DYNAMICS);	 break;
		// -----------------------------------------------------------------------------------------------
		
		case MC_PLUG_INS:		if (bDown) OnSwitchPlugin( ( m_cState.GetModifiers( M1_TO_M4_MASK ) == MCS_MODIFIER_M1 ) );		 break;
		case MC_BANK_DOWN:		if (bDown) OnHandleBankDownButton();						break;
		case MC_BANK_UP:		if (bDown) OnHandleBankUpButton();							break;
		case MC_CHANNEL_DOWN:	if (bDown) OnSwitchChannelDown();							break;
		case MC_CHANNEL_UP:		if (bDown) OnSwitchChannelUp();								break;
		case MC_FLIP:			if (bDown) OnSwitchFlip();									break;
		case MC_EDIT:			if (bDown) OnSwitchEdit();									break;
		case MC_NAME_VALUE:		if (bDown) OnSwitchNameValue();								break;
		case MC_SMPTE_BEATS:	if (bDown) OnSwitchSMPTEBeats();							break;
		case MC_F1:				if (bDown) OnSwitchF1();									break;
		case MC_F2:				if (bDown) OnSwitchF2();									break;
		case MC_F3:				if (bDown) OnSwitchF3();									break;
		case MC_F4:				if (bDown) OnSwitchF4();									break;
		case MC_F5:				if (bDown) OnSwitchF5();									break;
		case MC_F6:				if (bDown) OnSwitchF6();									break;
		case MC_F7:				if (bDown) OnSwitchF7();									break;
		case MC_F8:				if (bDown) OnSwitchF8();									break;
		case MC_NEW_AUDIO:		if (bDown) OnSwitchNewAudio();								break;
		case MC_NEW_MIDI:		if (bDown) OnSwitchNewMidi();								break;
		case MC_FIT_TRACKS:		if (bDown) OnSwitchFitTracks();								break;
		case MC_FIT_PROJECT:	if (bDown) OnSwitchFitProject();							break;
		case MC_OK_ENTER:		if (bDown) OnSwitchOKEnter();								break;
		case MC_CANCEL:			if (bDown) OnSwitchCancel();								break;
		case MC_NEXT_WIN:		if (bDown) OnSwitchNextWin();								break;
		case MC_CLOSE_WIN:		if (bDown) OnSwitchCloseWin();								break;
		case MC_M1:				// Fall through
		case MC_M2:				// Fall through
		case MC_M3:				// Fall through
		case MC_M4:				OnSwitchModifier(bD1 - MC_M1, bDown);						break;
		case MC_READ_OFF:		if (bDown) OnSwitchReadOff();								break;
		case MC_SNAPSHOT:		if (bDown) OnSwitchSnapshot();								break;
		case MC_TRACK:			if (bDown) OnSwitchTrack();									break;
		case MC_DISARM:			if (bDown) OnSwitchDisarm();								break;
		case MC_OFFSET:			if (bDown) OnSwitchOffset();								break;
		case MC_SAVE:			if (bDown) OnSwitchSave();									break;
		case MC_AUX:			if (bDown) OnSwitchAux();									break;
		case MC_MAIN:			if (bDown) OnSwitchMain();									break;
		case MC_UNDO:			if (bDown) OnSwitchUndo();									break;
		case MC_REDO:			if (bDown) OnSwitchRedo();									break;
		case MC_MARKER:			if (bDown) OnSwitchMarker();								break;
		case MC_LOOP:			if (bDown) OnSwitchLoop();									break;
		case MC_SELECT:			if (bDown) OnSwitchSelect();								break;
		case MC_PUNCH:			if (bDown) OnSwitchPunch();									break;
		case MC_JOG_PRM:		if (bDown) OnSwitchJogPrm();								break;		
		case MC_LOOP_ON_OFF:	OnSwitchLoopOnOff(bDown);									break;		
		case MC_HOME:			if (bDown) OnSwitchHome();									break;		
		case MC_REWIND:			OnSwitchRewind( bDown );									break;		
		case MC_FAST_FORWARD:	OnSwitchFastForward(bDown);									break;
		case MC_STOP:			if (bDown) OnSwitchStop();									break;		
		case MC_PLAY:			if (bDown) OnSwitchPlay();									break;		
		case MC_RECORD:			if ( bDown ) OnSwitchRecord();								break;
		case MC_CURSOR_UP:		OnSwitchCursorUp(bDown);									break;
		case MC_CURSOR_DOWN:	OnSwitchCursorDown(bDown);									break;
		case MC_CURSOR_LEFT:	OnSwitchCursorLeft(bDown);									break;
		case MC_CURSOR_RIGHT:	OnSwitchCursorRight(bDown);									break;
		case MC_CURSOR_ZOOM:	if (bDown) OnSwitchCursorZoom();							break;
		case MC_SCRUB:			OnHandleScrubButton(bDown);									break;
		case MC_USER_A:			if (bDown) OnSwitchUserA();									break;
		case MC_USER_B:			if (bDown) OnSwitchUserB();									break;
		case MC_FADER_MASTER:	OnSwitchMasterFader(bDown);									break;

		// ----------------------------------------------------------------
		// Delegated from CMackieControlXT if loop on/off button is pressed
		// 
		case MC_SOLO_1: case MC_SOLO_2: case MC_SOLO_3: case MC_SOLO_4:
		case MC_SOLO_5: case MC_SOLO_6: case MC_SOLO_7:
		case MC_SOLO_8: OnSwitchSolo( bD1, bDown );											break;

		case MC_MUTE_1: case MC_MUTE_2: case MC_MUTE_3: case MC_MUTE_4:
		case MC_MUTE_5: case MC_MUTE_6: case MC_MUTE_7:
		case MC_MUTE_8: OnSwitchMute( bD1, bDown );											break;

		case MC_REC_ARM_1: case MC_REC_ARM_2: case MC_REC_ARM_3: case MC_REC_ARM_4:
		case MC_REC_ARM_5: case MC_REC_ARM_6: case MC_REC_ARM_7:
		case MC_REC_ARM_8: OnSwitchRecArm( bD1, bDown );									break;
		// ----------------------------------------------------------------

		default:
			return false;
	}

	// It's one of ours, so record if it's currently pressed
	m_bSwitches[bD1] = bDown;

	// Keep a note of the last button pressed
	m_bLastButtonPressed = bD1;

	return true;
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSelectAssignment(Assignment eAssignment)
{
	if (MCS_ASSIGNMENT_EQ == eAssignment)
	{
		if (MCS_ASSIGNMENT_EQ_FREQ_GAIN == m_cState.GetAssignment())
		{
			m_cState.SetAssignment(MCS_ASSIGNMENT_EQ);
			m_cState.SetAssignmentMode(MCS_ASSIGNMENT_MULTI_CHANNEL);
			m_bWasInEQFreqGainMode = false;

			TempDisplaySelectedTrackName();

			return;
		}

		if (m_bWasInEQFreqGainMode ||
			(MCS_ASSIGNMENT_EQ == m_cState.GetAssignment() &&
			MCS_ASSIGNMENT_CHANNEL_STRIP == m_cState.GetAssignmentMode()))
		{
			m_cState.SetAssignment(MCS_ASSIGNMENT_EQ_FREQ_GAIN);
			m_bWasInEQFreqGainMode = true;

			TempDisplaySelectedTrackName();

			return;
		}
	}

	if (m_cState.GetAssignment() == eAssignment)
	{
		if (m_cState.GetAssignmentMode() == MCS_ASSIGNMENT_MULTI_CHANNEL)
			m_cState.SetAssignmentMode(MCS_ASSIGNMENT_CHANNEL_STRIP);
		else
			m_cState.SetAssignmentMode(MCS_ASSIGNMENT_MULTI_CHANNEL);

		m_bWasInEQFreqGainMode = false;
	}
	else
	{
		m_cState.SetAssignment(eAssignment);

		if (IsAPluginMode(eAssignment))
			TempDisplaySelectedTrackName();
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchBankDown()
{
	if ( (m_bSwitches[MC_LOOP_ON_OFF] || m_bSwitches[MC_SCRUB])
		&& m_cState.GetAssignment() == MCS_ASSIGNMENT_PLUGIN 
		&& m_cState.GetModifiers( BANK_CHANNEL_MASK ) == MCS_MODIFIER_EDIT )
	{
		ShiftPluginNumOffset( -1 );
		return;
	}

	switch (m_cState.GetModifiers(BANK_CHANNEL_MASK))
	{
		case MCS_MODIFIER_NONE:							// Go left by 8 tracks
			ShiftStripNumOffset(-NUM_MAIN_CHANNELS);
			break;

		case MCS_MODIFIER_M1:							// Go to first track
			ShiftStripNumOffset(-100000);
			break;

		case MCS_MODIFIER_EDIT:							// Go left by 8 parameters
			ShiftParamNumOffset(-NUM_MAIN_CHANNELS);
			break;

		case MCS_MODIFIER_M1 | MCS_MODIFIER_EDIT:		// Go to top/first parameter
			ShiftParamNumOffset(-100000);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchBankUp()
{
	if ( (m_bSwitches[MC_LOOP_ON_OFF] || m_bSwitches[MC_SCRUB]) 
		&& m_cState.GetAssignment() == MCS_ASSIGNMENT_PLUGIN 
		&& m_cState.GetModifiers( BANK_CHANNEL_MASK ) == MCS_MODIFIER_EDIT )
	{
		ShiftPluginNumOffset( 1 );
		return;
	}
		
	switch (m_cState.GetModifiers(BANK_CHANNEL_MASK))
	{
		case MCS_MODIFIER_NONE:							// Go right by 8 tracks
			ShiftStripNumOffset(NUM_MAIN_CHANNELS);
			break;

		case MCS_MODIFIER_M1:							// Go to last track
			ShiftStripNumOffset(100000);
			break;

		case MCS_MODIFIER_EDIT:							// Go right by 8 parameters
			ShiftParamNumOffset(NUM_MAIN_CHANNELS);
			break;

		case MCS_MODIFIER_M1 | MCS_MODIFIER_EDIT:		// Go to end/last parameter
			ShiftParamNumOffset(100000);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchChannelDown()
{
	switch (m_cState.GetModifiers(BANK_CHANNEL_MASK))
	{
		case MCS_MODIFIER_NONE:							// Go left by 1 track
			ShiftStripNumOffset(-1);
			break;

		case MCS_MODIFIER_M1:							// Assign Master fader to previous VMain
			SafeSetMasterFaderOffset(m_SwMasterFader.GetMixerStrip(), m_SwMasterFader.GetStripNum() - 1);
			TempDisplayMasterFader();
			break;

		case MCS_MODIFIER_EDIT:							// Go left by 1 parameter
			ShiftParamNumOffset(-1);
			break;

		case MCS_MODIFIER_M1 | MCS_MODIFIER_EDIT:		// Go left by 1 plugin
			ShiftPluginNumOffset(-1);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchChannelUp()
{
	switch (m_cState.GetModifiers(BANK_CHANNEL_MASK))
	{
		case MCS_MODIFIER_NONE:							// Go right by 1 track
			ShiftStripNumOffset(+1);
			break;

		case MCS_MODIFIER_M1:							// Assign Master fader to next VMain
			SafeSetMasterFaderOffset(m_SwMasterFader.GetMixerStrip(), m_SwMasterFader.GetStripNum() + 1);
			TempDisplayMasterFader();
			break;

		case MCS_MODIFIER_EDIT:							// Go right by 1 parameter
			ShiftParamNumOffset(+1);
			break;

		case MCS_MODIFIER_M1 | MCS_MODIFIER_EDIT:		// Go right by 1 plugin
			ShiftPluginNumOffset(+1);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchFlip()
{
	switch (m_cState.GetModifiers(M1_TO_M4_MASK))
	{
		case MCS_MODIFIER_NONE:							// Toggle flip mode
			m_cState.CycleFlipMode();
			break;

		case MCS_MODIFIER_M1:							// Toggle display flip
			m_cState.SetDisplayFlip(!m_cState.GetDisplayFlip());
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchEdit()
{
	switch (m_cState.GetModifiers(EDIT_MASK))
	{
		case MCS_MODIFIER_NONE:							// Enter edit mode
			m_cState.EnableModifier(MCS_MODIFIER_EDIT);
			break;

		case MCS_MODIFIER_M1:							// Enter numeric mode
		case MCS_MODIFIER_M1 | MCS_MODIFIER_EDIT:
			m_cState.EnableModifier(MCS_MODIFIER_NUMERIC);
			break;

		case MCS_MODIFIER_EDIT:							// Leave edit mode
			m_cState.DisableModifier(MCS_MODIFIER_EDIT);
			break;

		case MCS_MODIFIER_NUMERIC:						// Leave numeric mode
		case MCS_MODIFIER_M1 | MCS_MODIFIER_NUMERIC:
			m_cState.DisableModifier(MCS_MODIFIER_NUMERIC);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchNameValue()
{
	switch (m_cState.GetModifiers(M1_TO_M4_MASK))
	{
		case MCS_MODIFIER_NONE:							// Toggle displaying names/values
			m_cState.SetDisplayValues(!m_cState.GetDisplayValues());
			break;

		case MCS_MODIFIER_M1:							// Toggle display track numbers
			m_cState.SetDisplayTrackNumbers(!m_cState.GetDisplayTrackNumbers());
			break;

		case MCS_MODIFIER_M2:							// Toggle display level meters
			if (m_cState.HaveMeters())
			{
				switch (m_cState.GetDisplayLevelMeters())
				{
					case METERS_OFF:
						m_cState.SetDisplayLevelMeters(METERS_LEDS);
						break;

					case METERS_LEDS:
						m_cState.SetDisplayLevelMeters(METERS_BOTH);
						break;

					default:
					case METERS_BOTH:
						m_cState.SetDisplayLevelMeters(METERS_OFF);
						break;
				}
			}
			break;

		case MCS_MODIFIER_M3:							// Toggle display updates
			m_cState.SetDisableLCDUpdates(!m_cState.GetDisableLCDUpdates());
			break;

		case MCS_MODIFIER_M4:							// Force refresh of this module
			m_bForceRefreshWhenDone = true;
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchSMPTEBeats()
{
	switch (m_cState.GetModifiers(M1_TO_M4_MASK))
	{
		case MCS_MODIFIER_NONE:							// Toggle time display format
			m_cState.SetDisplaySMPTE(!m_cState.GetDisplaySMPTE());
			break;

		case MCS_MODIFIER_M1:							// Show Big Time window
			DoCommand(CMD_VIEW_BIG_TIME);
			break;

		case MCS_MODIFIER_M4:							// Force refresh of all modules
			m_bForceRefreshAllWhenDone = true;
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchF1()
{
	switch (m_cState.GetModifiers(FKEY_MASK))
	{
		case MCS_MODIFIER_NONE:							// User key F1
			if ((m_cState.GetUserFunctionKey(0) & KEYBINDING_KEYPRESS) > 0)
			{
				bool bShift;
				bool bAlt;
				bool bCtrl;
				int nVirtKey;
				VirtualKeys::CommandIdToVirtualKey(m_cState.GetUserFunctionKey(0), nVirtKey, bShift, bCtrl, bAlt);
				FakeKeyPress(bShift, bCtrl, bAlt, nVirtKey);
			}
			else
			{
				DoCommand(m_cState.GetUserFunctionKey(0));
			}
			break;

		case MCS_MODIFIER_M1:							// Cut
			DoCommand(CMD_EDIT_CUT);
			break;

		case MCS_MODIFIER_M2:							// Cut Special
			DoCommand(CMD_EDIT_CUT_WITH_DLG);
			break;

		case MCS_MODIFIER_M4:							// Reload the plug-in mappings
			m_cState.LoadPluginMappings();
			break;

		case MCS_MODIFIER_NUMERIC:						// Fake '1'
			FakeKeyPress(false, false, false, '1');
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchF2()
{
	switch (m_cState.GetModifiers(FKEY_MASK))
	{
		case MCS_MODIFIER_NONE:							// User key F2
			if ((m_cState.GetUserFunctionKey(1) & KEYBINDING_KEYPRESS) > 0)
			{
				bool bShift;
				bool bAlt;
				bool bCtrl;
				int nVirtKey;
				VirtualKeys::CommandIdToVirtualKey(m_cState.GetUserFunctionKey(1), nVirtKey, bShift, bCtrl, bAlt);
				FakeKeyPress(bShift, bCtrl, bAlt, nVirtKey);
			}
			else
			{
				DoCommand(m_cState.GetUserFunctionKey(1));
			}
			break;

		case MCS_MODIFIER_M1:							// Copy
			DoCommand(CMD_EDIT_COPY);
			break;

		case MCS_MODIFIER_M2:							// Copy Special
			DoCommand(CMD_EDIT_COPY_WITH_DLG);
			break;

		case MCS_MODIFIER_M3:							// Export Track Template
			DoExportTrackTemplate();
			break;

		case MCS_MODIFIER_M4:							// Copy Special
			DoCommand(CMD_EDIT_COPY_WITH_DLG);
			FakeKeyPress(false, false, false, VK_RETURN);
			break;

		case MCS_MODIFIER_NUMERIC:						// Fake '2'
			FakeKeyPress(false, false, false, '2');
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchF3()
{
	switch (m_cState.GetModifiers(FKEY_MASK))
	{
		case MCS_MODIFIER_NONE:							// User key F3
			if ((m_cState.GetUserFunctionKey(2) & KEYBINDING_KEYPRESS) > 0)
			{
				bool bShift;
				bool bAlt;
				bool bCtrl;
				int nVirtKey;
				VirtualKeys::CommandIdToVirtualKey(m_cState.GetUserFunctionKey(2), nVirtKey, bShift, bCtrl, bAlt);
				FakeKeyPress(bShift, bCtrl, bAlt, nVirtKey);
			}
			else
			{
				DoCommand(m_cState.GetUserFunctionKey(2));
			}
			break;

		case MCS_MODIFIER_M1:							// Paste
			DoCommand(CMD_EDIT_PASTE);
			break;

		case MCS_MODIFIER_M2:							// Paste Special
			DoCommand(CMD_EDIT_PASTE_SPECIAL);
			break;

		case MCS_MODIFIER_M3:							// Import Track Template
			DoImportTrackTemplate();
			break;

		case MCS_MODIFIER_M4:							// Paste Special & Press OK
			DoCommand(CMD_EDIT_PASTE_SPECIAL);
			FakeKeyPress(false, false, false, VK_RETURN);
			break;

		case MCS_MODIFIER_NUMERIC:						// Fake '3'
			FakeKeyPress(false, false, false, '3');
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchF4()
{
	switch (m_cState.GetModifiers(FKEY_MASK))
	{
		case MCS_MODIFIER_NONE:							// User key F4
			if ((m_cState.GetUserFunctionKey(3) & KEYBINDING_KEYPRESS) > 0)
			{
				bool bShift;
				bool bAlt;
				bool bCtrl;
				int nVirtKey;
				VirtualKeys::CommandIdToVirtualKey(m_cState.GetUserFunctionKey(3), nVirtKey, bShift, bCtrl, bAlt);
				FakeKeyPress(bShift, bCtrl, bAlt, nVirtKey);
			}
			else
			{
				DoCommand(m_cState.GetUserFunctionKey(3));
			}
			break;

		case MCS_MODIFIER_M1:							// Delete
			DoCommand(CMD_EDIT_DELETE);
			break;

		case MCS_MODIFIER_NUMERIC:						// Fake '4'
			FakeKeyPress(false, false, false, '4');
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchF5()
{
	switch (m_cState.GetModifiers(FKEY_MASK))
	{
		case MCS_MODIFIER_NONE:							// User key F5
			if ((m_cState.GetUserFunctionKey(4) & KEYBINDING_KEYPRESS) > 0)
			{
				bool bShift;
				bool bAlt;
				bool bCtrl;
				int nVirtKey;
				VirtualKeys::CommandIdToVirtualKey(m_cState.GetUserFunctionKey(4), nVirtKey, bShift, bCtrl, bAlt);
				FakeKeyPress(bShift, bCtrl, bAlt, nVirtKey);
			}
			else
			{
				DoCommand(m_cState.GetUserFunctionKey(4));
			}
			break;

		case MCS_MODIFIER_M1:							// Spacebar
			FakeKeyPress(false, false, false, VK_SPACE);
			break;

		case MCS_MODIFIER_NUMERIC:						// Fake '5'
			FakeKeyPress(false, false, false, '5');
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchF6()
{
	switch (m_cState.GetModifiers(FKEY_MASK))
	{
		case MCS_MODIFIER_NONE:							// User key F6
			if ((m_cState.GetUserFunctionKey(5) & KEYBINDING_KEYPRESS) > 0)
			{
				bool bShift;
				bool bAlt;
				bool bCtrl;
				int nVirtKey;
				VirtualKeys::CommandIdToVirtualKey(m_cState.GetUserFunctionKey(5), nVirtKey, bShift, bCtrl, bAlt);
				FakeKeyPress(bShift, bCtrl, bAlt, nVirtKey);
			}
			else
			{
				DoCommand(m_cState.GetUserFunctionKey(5));
			}
			break;

		case MCS_MODIFIER_M1:							// Alt
			FakeKeyPress(false, false, true, -1);
			break;

		case MCS_MODIFIER_NUMERIC:						// Fake '6'
			FakeKeyPress(false, false, false, '6');
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchF7()
{
	switch (m_cState.GetModifiers(FKEY_MASK))
	{
		case MCS_MODIFIER_NONE:							// User key F7
			if ((m_cState.GetUserFunctionKey(6) & KEYBINDING_KEYPRESS) > 0)
			{
				bool bShift;
				bool bAlt;
				bool bCtrl;
				int nVirtKey;
				VirtualKeys::CommandIdToVirtualKey(m_cState.GetUserFunctionKey(6), nVirtKey, bShift, bCtrl, bAlt);
				FakeKeyPress(bShift, bCtrl, bAlt, nVirtKey);
			}
			else
			{
				DoCommand(m_cState.GetUserFunctionKey(6));
			}
			break;

		case MCS_MODIFIER_M1:							// Tab
			FakeKeyPress(false, false, false, VK_TAB);
			break;

		case MCS_MODIFIER_M2:							// Shift-Tab
			FakeKeyPress(true, false, false, VK_TAB);
			break;

		case MCS_MODIFIER_NUMERIC:						// Fake '7'
			FakeKeyPress(false, false, false, '7');
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchF8()
{
	switch (m_cState.GetModifiers(FKEY_MASK))
	{
		case MCS_MODIFIER_NONE:							// User key F8
			if ((m_cState.GetUserFunctionKey(7) & KEYBINDING_KEYPRESS) > 0)
			{
				bool bShift;
				bool bAlt;
				bool bCtrl;
				int nVirtKey;
				VirtualKeys::CommandIdToVirtualKey(m_cState.GetUserFunctionKey(7), nVirtKey, bShift, bCtrl, bAlt);
				FakeKeyPress(bShift, bCtrl, bAlt, nVirtKey);
			}
			else
			{
				DoCommand(m_cState.GetUserFunctionKey(7));
			}
			break;

		case MCS_MODIFIER_M1:							// Backspace
			FakeKeyPress(false, false, false, VK_BACK);
			break;

		case MCS_MODIFIER_NUMERIC:						// Fake '8'
			FakeKeyPress(false, false, false, '8');
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchNewAudio()
{
	switch (m_cState.GetModifiers(FKEY_MASK))
	{
		case MCS_MODIFIER_NONE:							// Insert new audio track
			DoCommand(CMD_INSERT_AUDIO_TRACK);
			break;

		case MCS_MODIFIER_M1:							// Track | Clone
			DoCommand(CMD_TRACK_CLONE);
			break;

		case MCS_MODIFIER_M2:							// Track | Delete
			DoCommand(CMD_DELETE_TRACK);
			break;

		case MCS_MODIFIER_M3:							// Track | Wipe
			DoCommand(CMD_TRACK_WIPE);
			break;

		case MCS_MODIFIER_NUMERIC:						// Fake '9'
			FakeKeyPress(false, false, false, '9');
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchNewMidi()
{
	switch (m_cState.GetModifiers(FKEY_MASK))
	{
		case MCS_MODIFIER_NONE:							// Insert new MIDI track
			DoCommand(CMD_INSERT_MIDI_TRACK);
			break;

		case MCS_MODIFIER_M1:							// Track | Clone
			DoCommand(CMD_TRACK_CLONE);
			break;

		case MCS_MODIFIER_M2:							// Track | Delete
			DoCommand(CMD_DELETE_TRACK);
			break;

		case MCS_MODIFIER_M3:							// Track | Wipe
			DoCommand(CMD_TRACK_WIPE);
			break;

		case MCS_MODIFIER_NUMERIC:						// Fake '0'
			FakeKeyPress(false, false, false, '0');
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchFitTracks()
{
	switch (m_cState.GetModifiers(FKEY_MASK))
	{
		case MCS_MODIFIER_NONE:							// Fit tracks to window
			FakeKeyPress(false, false, false, 'F');
			break;

		case MCS_MODIFIER_M1:							// Show all tracks
			FakeKeyPress(false, false, false, 'A');
			break;

		case MCS_MODIFIER_M2:							// Show only selected tracks
			FakeKeyPress(false, false, false, 'H');
			break;

		case MCS_MODIFIER_M3:							// Show and fit selection
			FakeKeyPress(true, false, false, 'S');
			break;

		case MCS_MODIFIER_M4:							// Hide selected tracks
			FakeKeyPress(true, false, false, 'H');
			break;

		case MCS_MODIFIER_NUMERIC:						// Fake '.'
			FakeKeyPress(false, false, false, VK_DECIMAL);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchFitProject()
{
	switch (m_cState.GetModifiers(FKEY_MASK))
	{
		case MCS_MODIFIER_NONE:							// Fit project to window
			FakeKeyPress(true, false, false, 'F');
			break;

		case MCS_MODIFIER_NUMERIC:						// Fake '-'
			FakeKeyPress(false, false, false, VK_OEM_MINUS);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchOKEnter()
{
	switch (m_cState.GetModifiers(FKEY_MASK))
	{
		case MCS_MODIFIER_NONE:							// Fake 'Return'
			FakeKeyPress(false, false, false, VK_RETURN);
			break;

		case MCS_MODIFIER_NUMERIC:						// Fake 'Return'
			FakeKeyPress(false, false, false, VK_RETURN);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchCancel()
{
	switch (m_cState.GetModifiers(FKEY_MASK))
	{
		case MCS_MODIFIER_NONE:							// Cancel
			if ( m_pKeyboard )
				m_pKeyboard->NavigationKey(SNK_CANCEL);
			break;

		case MCS_MODIFIER_NUMERIC:						// Cancel
			if ( m_pKeyboard )
				m_pKeyboard->NavigationKey(SNK_CANCEL);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchNextWin()			// F15
{
	switch (m_cState.GetModifiers(FKEY_MASK))
	{
		case MCS_MODIFIER_NONE:							// Next window (Ctrl-Tab)
			FakeKeyPress(false, true, false, VK_TAB);
			break;

		case MCS_MODIFIER_M1:							// Previous window (Shift-Ctrl-Tab)
			FakeKeyPress(true, true, false, VK_TAB);
			break;

		case MCS_MODIFIER_M2:							// File | Info
			DoCommand(CMD_FILE_INFO);
			break;

		case MCS_MODIFIER_NUMERIC:						// Tab
			FakeKeyPress(false, false, false, VK_TAB);
			break;

		case MCS_MODIFIER_M1 | MCS_MODIFIER_NUMERIC:	// Shift-Tab
			FakeKeyPress(true, false, false, VK_TAB);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchCloseWin()			// F16
{
	switch (m_cState.GetModifiers(FKEY_MASK))
	{
		case MCS_MODIFIER_NONE:							// Close window (Ctrl-F4)
			FakeKeyPress(false, true, false, VK_F4);
			break;

		case MCS_MODIFIER_M1:							// Backspace
			FakeKeyPress(false, false, false, VK_BACK);
			break;

		case MCS_MODIFIER_M2:							// Spacebar
			FakeKeyPress(false, false, false, VK_SPACE);
			break;

		case MCS_MODIFIER_NUMERIC:						// Backspace
			FakeKeyPress(false, false, false, VK_BACK);
			break;

		case MCS_MODIFIER_M1 | MCS_MODIFIER_NUMERIC:	// Spacebar
			FakeKeyPress(false, false, false, VK_SPACE);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchModifier(BYTE bN, bool bDown)
{
	DWORD dwMask = (1 << bN);
	
	if (bDown)											// Modifier on
		m_cState.EnableModifier(dwMask);
	else												// Modifier off
		m_cState.DisableModifier(dwMask);
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchReadOff()
{
	switch (m_cState.GetModifiers(M1_TO_M4_MASK))
	{
		case MCS_MODIFIER_NONE:							// Enable/Disable Automation Playback
			DoCommand(NEW_CMD_ENABLE_DISABLE_AUTOMATION_PLAYBACK);
			break;

		case MCS_MODIFIER_M1:							// Toggle disable faders
			m_cState.SetDisableFaders(!m_cState.GetDisableFaders());
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchSnapshot()
{
	switch (m_cState.GetModifiers(M1_TO_M4_MASK))
	{
		case MCS_MODIFIER_NONE:							// Take an automation snapshot of all armed controls
			DoCommand(NEW_CMD_SNAPSHOT);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSelectMixerStrip(SONAR_MIXER_STRIP eMixerStrip)
{
	switch (m_cState.GetModifiers(M1_TO_M4_MASK))
	{
		case MCS_MODIFIER_M1: // needed for switching to MIX_STRIP_RACK
		case MCS_MODIFIER_NONE:							// Set mixer strip type
			m_cState.SetMixerStrip(eMixerStrip);

			TempDisplaySelectedTrackName();
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchDisarm()
{
	switch (m_cState.GetModifiers(M1_TO_M4_MASK))
	{
		case MCS_MODIFIER_NONE:							// Disarm all armed parameters
			DoCommand(NEW_CMD_DISARM_ALL_AUTOMATION_CONTROLS);
			break;

		case MCS_MODIFIER_M1:							// Disarm all tracks
			DisarmAllTracks();
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchOffset()
{
	switch (m_cState.GetModifiers(M1_TO_M4_MASK))
	{
		case MCS_MODIFIER_NONE:							// Toggle offset mode
			DoCommand(NEW_CMD_ENVELOPE_OFFSET_MODE);
			break;

		case MCS_MODIFIER_M3:							// Set all faders to default
			RequestSetAllFadersToDefault();
			break;

		case MCS_MODIFIER_M4:							// Set all VPots to default
			RequestSetAllVPotsToDefault();
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchSave()
{
	switch (m_cState.GetModifiers(M1_TO_M4_MASK))
	{
		case MCS_MODIFIER_NONE:							// File | Save
			DoCommand(CMD_FILE_SAVE);
			break;

		case MCS_MODIFIER_M1:							// File | Save As...
			DoCommand(CMD_FILE_SAVE_AS);
			break;

		case MCS_MODIFIER_M2:
			DoCommand(CMD_FILE_SAVE_COPY_AS);
			break;

		case MCS_MODIFIER_M3:
			DoCommand(CMD_AUDIO_MIXDOWN_EXPORT);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchUndo()
{
	switch (m_cState.GetModifiers(M1_TO_M4_MASK))
	{
		case MCS_MODIFIER_NONE:							// Edit | Undo
			DoCommand(CMD_EDIT_UNDO);
			break;

		case MCS_MODIFIER_M1:							// Edit | Undo History
			DoCommand(CMD_EDIT_HISTORY);
			break;

		case MCS_MODIFIER_M2:							// Undo View Change (U)
			FakeKeyPress(false, false, false, 'U');
			break;

		case MCS_MODIFIER_M3:							// Reject Loop Take
			DoCommand(CMD_REALTIME_REJECT_LOOP_TAKE);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchRedo()
{
	switch (m_cState.GetModifiers(M1_TO_M4_MASK))
	{
		case MCS_MODIFIER_NONE:							// Edit | Redo
			DoCommand(CMD_EDIT_REDO);
			break;

		case MCS_MODIFIER_M2:							// Redo View Change (Shift+U)
			FakeKeyPress(true, false, false, 'U');
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchMarker()
{
	switch (m_cState.GetModifiers(M1_TO_M4_MASK))
	{
		case MCS_MODIFIER_NONE:							// Set navigation mode
			OnSelectNavigationMode(MCS_NAVIGATION_MARKER);
			break;

		case MCS_MODIFIER_M1:							// Insert marker
			DoCommand(CMD_INSERT_MARKER);
			break;

		case MCS_MODIFIER_M2:							// View markers
			DoCommand(CMD_VIEW_MARKERS);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchLoop()
{
	switch (m_cState.GetModifiers(M1_TO_M4_MASK))
	{
		case MCS_MODIFIER_NONE:							// Set navigation mode
			OnSelectNavigationMode(MCS_NAVIGATION_LOOP);
			break;

		case MCS_MODIFIER_M1:							// Transport | Loop / Auto Shuttle
			DoCommand(CMD_REALTIME_AUTO_SHUTTLE);
			break;

		case MCS_MODIFIER_M2:							// Set loop from selection
			DoCommand(CMD_SET_LOOP_FROM_SELECTION);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchSelect()
{
	switch (m_cState.GetModifiers(M1_TO_M4_MASK))
	{
		case MCS_MODIFIER_NONE:							// Set navigation mode
			OnSelectNavigationMode(MCS_NAVIGATION_SELECT);
			break;

		case MCS_MODIFIER_M1:							// Select current track
			FakeKeyPress(false, false, false, VK_OEM_COMMA);
			break;

		case MCS_MODIFIER_M2:							// Extend select current track
			FakeKeyPress(true, false, false, VK_OEM_COMMA);
			break;

		case MCS_MODIFIER_M3:							// Edit | Select All
			DoCommand(CMD_SELECT_ALL);
			break;

		case MCS_MODIFIER_M4:							// Edit | Select None
			DoCommand(CMD_SELECT_NONE);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchPunch()
{
	switch (m_cState.GetModifiers(M1_TO_M4_MASK))
	{
		case MCS_MODIFIER_NONE:							// Set navigation mode
			OnSelectNavigationMode(MCS_NAVIGATION_PUNCH);
			break;

		case MCS_MODIFIER_M1:							// Transport | Record Options
			DoCommand(CMD_REALTIME_RECORD_MODE);
			break;

		case MCS_MODIFIER_M2:							// Set punch from selection
			DoCommand(CMD_SET_PUNCH_FROM_SELECTION);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSelectNavigationMode(NavigationMode eNavigationMode)
{
	if (m_cState.GetNavigationMode() == eNavigationMode)
		m_cState.SetNavigationMode(MCS_NAVIGATION_NORMAL);
	else
		m_cState.SetNavigationMode(eNavigationMode);
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchJogPrm()
{
	switch (m_cState.GetModifiers(M1_TO_M4_MASK))
	{
		case MCS_MODIFIER_NONE:							// Toggle jog parameter mode
			m_cState.SetJogParamMode(!m_cState.GetJogParamMode());
			break;

		case MCS_MODIFIER_M1:							// Set jog resolution to measures
			m_cState.SetJogResolution(JOG_MEASURES);
			break;

		case MCS_MODIFIER_M2:							// Set jog resolution to beats
			m_cState.SetJogResolution(JOG_BEATS);
			break;

		case MCS_MODIFIER_M3:							// Set jog resolution to ticks
			m_cState.SetJogResolution(JOG_TICKS);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchLoopOnOff(bool bDown)
{
	// react to switch up, so LOOP button can be used as shift
	if ( (!bDown) && (m_bLastButtonPressed == MC_LOOP_ON_OFF ) )
	{
		switch ( m_cState.GetModifiers( M1_TO_M4_MASK ) )
		{
			case MCS_MODIFIER_NONE:							// Toggle loop mode
				ToggleLoopMode();
				break;

			case MCS_MODIFIER_M1:							// Transport | Loop / Auto Shuttle
				DoCommand( CMD_REALTIME_AUTO_SHUTTLE );
				break;

			default:
				break;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchHome()
{
	switch (m_cState.GetModifiers(M1_TO_M4_MASK))
	{
		case MCS_MODIFIER_NONE:							// Goto start
			DoCommand(CMD_GOTO_START);
			break;

		case MCS_MODIFIER_M1:							// Goto end
			DoCommand(CMD_GOTO_END);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchRewind(bool bDown)
{
	// Send LED to HUI right away to make it seem more responsive
	if ( bDown && UsingHUIProtocol() )
		SetHuiLED( MC_REWIND, 1, true );

	// LOOP + REW = Go to start
	if ( bDown && (m_bSwitches[MC_LOOP_ON_OFF]))
	{
		DoCommand( CMD_GOTO_START );
	}
	else
	{
		ClearTransportCallbackTimer();

		if ( bDown )
		{
			switch ( m_cState.GetModifiers( M1_TO_M4_MASK ) )
			{
				case MCS_MODIFIER_NONE:						// Move backwards
					MoveRewindOrFastForward( DIR_BACKWARD );
					break;

				case MCS_MODIFIER_M1:						// Set left marker
					SetRewindOrFastForward( DIR_BACKWARD );
					break;

				default:
					break;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchFastForward(bool bDown)
{
	// Send LED to HUI right away to make it seem more responsive
	if ( bDown && UsingHUIProtocol() )
		SetHuiLED( MC_FAST_FORWARD, 1, true );

	ClearTransportCallbackTimer();

	if (bDown)
	{
		switch (m_cState.GetModifiers(M1_TO_M4_MASK))
		{
			case MCS_MODIFIER_NONE:						// Move forwards
				MoveRewindOrFastForward(DIR_FORWARD);
				break;

			case MCS_MODIFIER_M1:						// Set right marker
				SetRewindOrFastForward(DIR_FORWARD);
				break;

			default:
				break;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchStop()
{
	// Send LED to HUI right away to make it seem more responsive
	if (UsingHUIProtocol()) 
		SetHuiLED( MC_STOP, 1, true );

	ClearTransportCallbackTimer();

	switch (m_cState.GetModifiers(M1_TO_M4_MASK))
	{
		case MCS_MODIFIER_NONE:							// Stop
			DoCommand(CMD_REALTIME_STOP);
			break;

		case MCS_MODIFIER_M1:							// Transport | Reset
			DoCommand(CMD_REALTIME_PANIC);
			break;

		case MCS_MODIFIER_M2:							// Reject Loop Take
			DoCommand(CMD_REALTIME_REJECT_LOOP_TAKE);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchPlay()
{
	// Send LED to HUI right away to make it seem more responsive
	if (UsingHUIProtocol()) 
		SetHuiLED( MC_PLAY, 1, true );

	ClearTransportCallbackTimer();

	switch (m_cState.GetModifiers(M1_TO_M4_MASK))
	{
		case MCS_MODIFIER_NONE:							// Play
			OnPlayPressed();
			break;

		case MCS_MODIFIER_M1:							// Run Audio
			DoCommand(CMD_REALTIME_AUDIO_RUNNING);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchRecord()
{
	if ( (m_bSwitches[MC_LOOP_ON_OFF] || m_bSwitches[MC_SCRUB]) ) // Loop/REC on nanoKONTROL does undo
	{
		DoCommand( CMD_EDIT_UNDO );
	}
	else
	{
		// Send LED to HUI right away to make it seem more responsive
		if ( UsingHUIProtocol() ) 
			SetHuiLED( MC_RECORD, 1, true );

		ClearTransportCallbackTimer();

		switch ( m_cState.GetModifiers( M1_TO_M4_MASK ) )
		{
			case MCS_MODIFIER_NONE:							// Record
				DoCommand( CMD_REALTIME_RECORD );
				break;

			case MCS_MODIFIER_M1:							// Record Automation
				DoCommand( CMD_REALTIME_RECORD_AUTOMATION );
				break;

			case MCS_MODIFIER_M2:							// Transport | Record Options
				DoCommand( CMD_REALTIME_RECORD_MODE );
				break;

			default:
				break;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchCursorUp(bool bDown)
{
	ClearKeyRepeatCallbackTimer();

	if (bDown)
		DoCursorKey(MC_CURSOR_UP);
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchCursorDown(bool bDown)
{
	ClearKeyRepeatCallbackTimer();

	if (bDown)
		DoCursorKey(MC_CURSOR_DOWN);
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchCursorLeft(bool bDown)
{
	ClearKeyRepeatCallbackTimer();

	if (bDown)
		DoCursorKey(MC_CURSOR_LEFT);
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchCursorRight(bool bDown)
{
	ClearKeyRepeatCallbackTimer();

	if (bDown)
		DoCursorKey(MC_CURSOR_RIGHT);
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchCursorZoom()
{
	ClearKeyRepeatCallbackTimer();

	switch (m_cState.GetModifiers(ZOOM_MASK))
	{
		case MCS_MODIFIER_NONE:							// Enter zoom mode
			m_cState.EnableModifier(MCS_MODIFIER_ZOOM);
			break;

		case MCS_MODIFIER_ZOOM:							// Leave zoom mode
			m_cState.DisableModifier(MCS_MODIFIER_ZOOM);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchScrub()
{
	switch (m_cState.GetModifiers(M1_TO_M4_MASK))
	{
		case MCS_MODIFIER_NONE:							// Toggle scrub mode
			ToggleScrubMode();
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchUserA()
{
	if ((m_cState.GetUserFootSwitch(0) & KEYBINDING_KEYPRESS) > 0)
	{
		bool bShift;
		bool bAlt;
		bool bCtrl;
		int nVirtKey;
		VirtualKeys::CommandIdToVirtualKey(m_cState.GetUserFootSwitch(0), nVirtKey, bShift, bCtrl, bAlt);
		FakeKeyPress(bShift, bCtrl, bAlt, nVirtKey);
	}
	else
	{
		DoCommand(m_cState.GetUserFootSwitch(0));			// User foot switch A
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchUserB()
{
	if ((m_cState.GetUserFootSwitch(1) & KEYBINDING_KEYPRESS) > 0)
	{
		bool bShift;
		bool bAlt;
		bool bCtrl;
		int nVirtKey;
		VirtualKeys::CommandIdToVirtualKey(m_cState.GetUserFootSwitch(1), nVirtKey, bShift, bCtrl, bAlt);
		FakeKeyPress(bShift, bCtrl, bAlt, nVirtKey);
	}
	else
	{
		DoCommand(m_cState.GetUserFootSwitch(1));			// User foot switch B
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchMasterFader(bool bDown)
{
	if (bDown)											// Fader touched
	{
		m_SwMasterFader.TouchCapture();
	}
	else												// Fader released
	{
		m_SwMasterFader.TouchRelease();

		// When the user stops touching the fader knob, the host should send the
		// last fader position back to Mackie Control, or else the fader will snap
		// back to the last received position (while the knob is touched incoming
		// fader positions are ignored, so this will be the last position before
		// it was touched)

		// We should probably arrange for this to be sent on the next refresh,
		// but this is simpler
		UpdateMasterFader(true);
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::TransportTimerCallback()
{
	CCriticalSectionAuto csa1(&m_cs);
	CCriticalSectionAuto csa2(m_cState.GetCS());

	MoveRewindOrFastForward(m_eCurrentTransportDir);
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::MoveRewindOrFastForward(Direction eDir)
{
	m_eCurrentTransportDir = eDir;

	switch (m_cState.GetNavigationMode())
	{
		case MCS_NAVIGATION_NORMAL:
			NudgeTimeCursor(m_cState.GetTransportResolution(), eDir);
			SetTransportCallbackTimer(0.25f, 300, 15);
			break;

		case MCS_NAVIGATION_MARKER:
			if (m_bSwitches[MC_LOOP_ON_OFF] || m_bSwitches[MC_SCRUB])
				DoCommand(CMD_INSERT_MARKER);
			else
			{
				DoCommand(DIR_BACKWARD == eDir ? CMD_MARKER_PREVIOUS : CMD_MARKER_NEXT);
				SetTransportCallbackTimer(0.25f, 200, 100);
			}
			break;

		case MCS_NAVIGATION_LOOP:
			SetTimeCursor(DIR_BACKWARD == eDir ? TRANSPORT_TIME_LOOP_IN : TRANSPORT_TIME_LOOP_OUT);
			break;
		
		case MCS_NAVIGATION_SELECT:
			DoCommand(DIR_BACKWARD == eDir ? CMD_GOTO_FROM : CMD_GOTO_THRU);
			break;

		case MCS_NAVIGATION_PUNCH:
			SetTimeCursor(DIR_BACKWARD == eDir ? TRANSPORT_TIME_PUNCH_IN : TRANSPORT_TIME_PUNCH_OUT);
			break;

		default:
			// This fixes the issue where RWD doesn't work when play was started via GUI, and stop was done via CS
			// TODO: work out how it ever gets to this state
			NudgeTimeCursor(m_cState.GetTransportResolution(), eDir);
			SetTransportCallbackTimer(0.25f, 300, 15);
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::SetRewindOrFastForward(Direction eDir)
{
	switch (m_cState.GetNavigationMode())
	{
		case MCS_NAVIGATION_NORMAL:
			DoCommand(DIR_BACKWARD == eDir ? CMD_GOTO_START : CMD_GOTO_END);
			break;

		case MCS_NAVIGATION_MARKER:
			DoCommand(CMD_INSERT_MARKER);
			break;

		case MCS_NAVIGATION_LOOP:
			SetMarker(DIR_BACKWARD == eDir ? TRANSPORT_TIME_LOOP_IN : TRANSPORT_TIME_LOOP_OUT);
			break;
		
		case MCS_NAVIGATION_SELECT:
			DoCommand(DIR_BACKWARD == eDir ? CMD_FROM_EQ_NOW : CMD_THRU_EQ_NOW);
			break;

		case MCS_NAVIGATION_PUNCH:
			SetMarker(DIR_BACKWARD == eDir ? TRANSPORT_TIME_PUNCH_IN : TRANSPORT_TIME_PUNCH_OUT);
			break;

		default:
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::KeyRepeatTimerCallback()
{
	CCriticalSectionAuto csa1(&m_cs);
	CCriticalSectionAuto csa2(m_cState.GetCS());

	DoCursorKey(m_bKeyRepeatKey);
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::DoCursorKey(BOOL bKey)
{
	m_bKeyRepeatKey = bKey;

	bool bDoRepeat = false;

	switch (bKey)
	{
		case MC_CURSOR_UP:
			bDoRepeat = DoCursorKeyUp();
			break;

		case MC_CURSOR_DOWN:
			bDoRepeat = DoCursorKeyDown();
			break;

		case MC_CURSOR_LEFT:
			bDoRepeat = DoCursorKeyLeft();
			break;

		case MC_CURSOR_RIGHT:
			bDoRepeat = DoCursorKeyRight();
			break;

		default:
			return;
	}

	if ( bDoRepeat )
		SetKeyRepeatCallbackTimer(0.4f, 500, 50);
}

/////////////////////////////////////////////////////////////////////////////

bool CMackieControlMaster::DoCursorKeyUp()
{
	bool bDoRepeat = true;

	switch (m_cState.GetModifiers(ZOOM_MASK))
	{
		case MCS_MODIFIER_NONE:							// Cursor up
			if ( m_cState.GetAssignment() == MCS_ASSIGNMENT_PLUGIN && m_cState.GetModifiers( BANK_CHANNEL_MASK ) == MCS_MODIFIER_EDIT )
			{
				bDoRepeat = false;
				ShiftPluginNumOffset( 1 );
			}
			else
				FakeKeyPress(false, false, false, VK_UP);
			break;

		case MCS_MODIFIER_M1:							// Page Up
			FakeKeyPress(false, false, false, VK_PRIOR);
			break;

		case MCS_MODIFIER_M2:							// Home
			FakeKeyPress(false, false, false, VK_HOME);
			break;

		case MCS_MODIFIER_NONE | MCS_MODIFIER_ZOOM:		// Decrease track height
			FakeKeyPress(false, true, false, VK_UP);
			break;

		case MCS_MODIFIER_M1 | MCS_MODIFIER_ZOOM:		// Scale waveforms all tracks
			FakeKeyPress(false, false, true, VK_UP);
			break;

		case MCS_MODIFIER_M2 | MCS_MODIFIER_ZOOM:		// Scale waveforms current track
			FakeKeyPress(false, true, true, VK_UP);
			break;

		case MCS_MODIFIER_M3 | MCS_MODIFIER_ZOOM:		// Zoom current track out vertically
			FakeKeyPress(true, true, false, VK_UP);
			break;

		case MCS_MODIFIER_M4 | MCS_MODIFIER_ZOOM:		// Fit tracks to window
			FakeKeyPress(false, false, false, 'F');
			break;

		default:
			bDoRepeat = false;
			break;
	}

	return bDoRepeat;
}

/////////////////////////////////////////////////////////////////////////////

bool CMackieControlMaster::DoCursorKeyDown()
{
	bool bDoRepeat = true;

	switch (m_cState.GetModifiers(ZOOM_MASK))
	{
		case MCS_MODIFIER_NONE:							// Cursor down
			if ( m_cState.GetAssignment() == MCS_ASSIGNMENT_PLUGIN && m_cState.GetModifiers( BANK_CHANNEL_MASK ) == MCS_MODIFIER_EDIT )
			{
				bDoRepeat = false;
				ShiftPluginNumOffset( -1 );
			}
			else
				FakeKeyPress( false, false, false, VK_DOWN );
			break;

		case MCS_MODIFIER_M1:							// Page Down
			FakeKeyPress(false, false, false, VK_NEXT);
			break;

		case MCS_MODIFIER_M2:							// End
			FakeKeyPress(false, false, false, VK_END);
			break;

		case MCS_MODIFIER_NONE | MCS_MODIFIER_ZOOM:		// Increase track height
			FakeKeyPress(false, true, false, VK_DOWN);
			break;

		case MCS_MODIFIER_M1 | MCS_MODIFIER_ZOOM:		// Scale waveforms all tracks
			FakeKeyPress(false, false, true, VK_DOWN);
			break;

		case MCS_MODIFIER_M2 | MCS_MODIFIER_ZOOM:		// Scale waveforms current track
			FakeKeyPress(false, true, true, VK_DOWN);
			break;

		case MCS_MODIFIER_M3 | MCS_MODIFIER_ZOOM:		// Zoom current track in vertically
			FakeKeyPress(true, true, false, VK_DOWN);
			break;

		default:
			bDoRepeat = false;
			break;
	}

	return bDoRepeat;
}

/////////////////////////////////////////////////////////////////////////////

bool CMackieControlMaster::DoCursorKeyLeft()
{
	bool bDoRepeat = true;

	switch (m_cState.GetModifiers(ZOOM_MASK))
	{
		case MCS_MODIFIER_NONE:							// Cursor left
			if ( m_cState.GetAssignment() == MCS_ASSIGNMENT_PLUGIN && m_cState.GetModifiers( BANK_CHANNEL_MASK ) == MCS_MODIFIER_EDIT )
			{
				bDoRepeat = false;
				ShiftParamNumOffset( -NUM_MAIN_CHANNELS );
				m_dwToolbarUpdateCount++;
			}
			else
				FakeKeyPress(false, false, false, VK_LEFT);
			break;

		case MCS_MODIFIER_M1:							// Shift-Tab
			if ( m_cState.GetAssignment() == MCS_ASSIGNMENT_PLUGIN )
				ShiftParamNumOffset( -1 );
			else
				FakeKeyPress(true, false, false, VK_TAB);
			break;

		case MCS_MODIFIER_M2:							// Backspace
			FakeKeyPress(false, false, false, VK_BACK);
			break;

		case MCS_MODIFIER_NONE | MCS_MODIFIER_ZOOM:		// Zoom out horizontally
			FakeKeyPress(false, true, false, VK_LEFT);
			break;

		default:
			bDoRepeat = false;
			break;
	}

	return bDoRepeat;
}

/////////////////////////////////////////////////////////////////////////////

bool CMackieControlMaster::DoCursorKeyRight()
{
	bool bDoRepeat = true;

	switch (m_cState.GetModifiers(ZOOM_MASK))
	{
		case MCS_MODIFIER_NONE:							// Cursor right
			if ( m_cState.GetAssignment() == MCS_ASSIGNMENT_PLUGIN && m_cState.GetModifiers( BANK_CHANNEL_MASK ) == MCS_MODIFIER_EDIT )
			{
				bDoRepeat = false;
				ShiftParamNumOffset( NUM_MAIN_CHANNELS );
				m_dwToolbarUpdateCount++;
			}
			else
				FakeKeyPress(false, false, false, VK_RIGHT);
			break;

		case MCS_MODIFIER_M1:							// Tab
			if ( m_cState.GetAssignment() == MCS_ASSIGNMENT_PLUGIN )
				ShiftParamNumOffset( 1 );
			else
				FakeKeyPress(false, false, false, VK_TAB);
			break;

		case MCS_MODIFIER_M2:							// Spacebar
			FakeKeyPress(false, false, false, VK_SPACE);
			break;

		case MCS_MODIFIER_NONE | MCS_MODIFIER_ZOOM:		// Zoom in horizontally
			FakeKeyPress(false, true, false, VK_RIGHT);
			break;

		case MCS_MODIFIER_M4 | MCS_MODIFIER_ZOOM:		// Fit project to window
			FakeKeyPress(true, false, false, 'F');
			break;

		default:
			bDoRepeat = false;
			break;
	}

	return bDoRepeat;
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnHandleScrubButton(bool bDown)
{
	// If ScrubBankSelectsTrackBus is checked, then the scrub button is used as a shift key in conjunction with
	// the Bank up/down buttons, so wait until the key is "up" to switch scrub mode.
	if (m_cState.GetScrubBankSelectsTrackBus())
	{
		// Only switch scrub mode if this is a scrub button "up", and 
		// the last "down" button pressed was also the scrub button
		if ((!bDown) && (m_bLastButtonPressed == MC_SCRUB))
			OnSwitchScrub();
	}
	else if (bDown)
		OnSwitchScrub();

	m_bScrubKeyDown = bDown;
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnHandleBankDownButton()
{
	// If the scrub button is down, switch to controlling tracks
	if (((m_cState.GetScrubBankSelectsTrackBus()) && (m_bScrubKeyDown)) || (m_bSwitches[MC_LOOP_ON_OFF]))
		OnSelectMixerStrip(MIX_STRIP_TRACK);
	else
		OnSwitchBankDown();
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnHandleBankUpButton()
{
	// If the scrub button is down, switch to controlling buses
	if (((m_cState.GetScrubBankSelectsTrackBus()) && (m_bScrubKeyDown)) || (m_bSwitches[MC_LOOP_ON_OFF]))
		OnSelectMixerStrip(m_cState.BusType());
	else
		OnSwitchBankUp();
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::DoExportTrackTemplate()
{	
	while(PathFileExistsW(m_cbTempTrackTemplateFilename))
		DeleteFileW(m_cbTempTrackTemplateFilename);

	WaitForInputIdle(GetCurrentProcess(), 10000);

	HGLOBAL hglbCopy;
	// Open the clip board and set the data in to the clip board.
	if (::OpenClipboard(0))
	{
		::EmptyClipboard();
		wchar_t *wcBuffer = 0;
		hglbCopy = GlobalAlloc(GMEM_MOVEABLE,
			(m_cbTempTrackTemplateFilename.GetLength() + 1) * sizeof(wchar_t));

		wcBuffer = (wchar_t*)GlobalLock(hglbCopy);
		wcscpy(wcBuffer, m_cbTempTrackTemplateFilename);

		GlobalUnlock(hglbCopy);
		::SetClipboardData(CF_UNICODETEXT, hglbCopy);
		::CloseClipboard();
	}

	DoCommand(CMD_FILE_EXPORT_TRACK_TEMPLATE);
	WaitForInputIdle(GetCurrentProcess(), 10000);

	FakeKeyPress(false, false, false, VK_BACK);
	FakeKeyPress(false, false, false, VK_SPACE);
	FakeKeyPress(false, false, false, VK_BACK);
	FakeKeyPress(false, true, false, 'V');
	WaitForInputIdle(GetCurrentProcess(), 10000);
	FakeKeyPress(false, false, false, VK_RETURN);
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::DoImportTrackTemplate()
{
	HGLOBAL hglbCopy;
	// Open the clip board and set the data in to the clip board.
	if (::OpenClipboard(0))
	{
		::EmptyClipboard();
		wchar_t *wcBuffer = 0;
		hglbCopy = GlobalAlloc(GMEM_MOVEABLE,
			(m_cbTempTrackTemplateFilename.GetLength() + 1) * sizeof(wchar_t));

		wcBuffer = (wchar_t*)GlobalLock(hglbCopy);
		wcscpy(wcBuffer, m_cbTempTrackTemplateFilename);

		GlobalUnlock(hglbCopy);
		::SetClipboardData(CF_UNICODETEXT, hglbCopy);
		::CloseClipboard();
	}
	DoCommand(CMD_FILE_IMPORT_TRACK_TEMPLATE);
	WaitForInputIdle(GetCurrentProcess(), 10000);
	FakeKeyPress(false, false, false, VK_BACK);
	FakeKeyPress(false, false, false, VK_SPACE);
	FakeKeyPress(false, false, false, VK_BACK);
	FakeKeyPress(false, true, false, 'V');
	WaitForInputIdle(GetCurrentProcess(), 10000);
	FakeKeyPress(false, false, false, VK_RETURN);
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchTrack()
{
	switch (m_cState.GetModifiers(M1_TO_M4_MASK))
	{
		case MCS_MODIFIER_NONE:							// Select Tracks
			{
				SONAR_MIXER_STRIP eInitialMixerStrip = m_cState.GetMixerStrip();
				
				OnSelectMixerStrip( MIX_STRIP_TRACK );
				
				if ( eInitialMixerStrip == MIX_STRIP_RACK )
					RestorePreSynthRackAssignments();
			}
			break;

		case MCS_MODIFIER_M1:
			DoCommand(CMD_FILE_EXPORT_TRACK_TEMPLATE);  // Export Track Template
			break;

		case MCS_MODIFIER_M2:
			DoCommand(CMD_FILE_IMPORT_TRACK_TEMPLATE);  // Import Track Template
			break;

		case MCS_MODIFIER_M3:							// Bounce to Track(s) ??
			DoCommand(CMD_AUDIO_MIXDOWN_BOUNCE);
			break;

		case MCS_MODIFIER_M4:							// Bounce to Clip(s)
			DoCommand(CMD_BOUNCE_TO_CLIPS);
			break;

		default:
			break;
	}
	
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchAux()
{
	SONAR_MIXER_STRIP eInitialMixerStrip = m_cState.GetMixerStrip();

	OnSelectMixerStrip( m_cState.BusType() );
	
	if ( eInitialMixerStrip == MIX_STRIP_RACK )
		RestorePreSynthRackAssignments();
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchMain()
{
	SONAR_MIXER_STRIP eInitialMixerStrip = m_cState.GetMixerStrip();

	OnSelectMixerStrip( m_cState.MasterType() );

	if ( eInitialMixerStrip == MIX_STRIP_RACK )
		RestorePreSynthRackAssignments();
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchSolo( BYTE nSwitch, bool bDown )
{
	if ( bDown && (m_bSwitches[MC_LOOP_ON_OFF] || m_bSwitches[MC_SCRUB]))
	{
		SONAR_MIXER_STRIP eInitialMixerStrip = m_cState.GetMixerStrip();

		switch ( nSwitch )
		{
			case MC_SOLO_1:	
				OnSwitchTrack();		
				break;

			case MC_SOLO_2:	
				OnSwitchAux();			
				break;

			case MC_SOLO_3:	
				OnSwitchMain();			
				break;

			case MC_SOLO_4: 
				OnSwitchPlugin( true );	
				break;

			case MC_SOLO_7: 
				if ( eInitialMixerStrip != MIX_STRIP_RACK )
				{
					OnSelectAssignment( MCS_ASSIGNMENT_PARAMETER );
					m_cState.DisableModifier( MCS_MODIFIER_EDIT );
				}
				break;

			case MC_SOLO_8:
				OnSwitchPlugin( false );  
				m_cState.EnableModifier( MCS_MODIFIER_EDIT );
				break;
		}
	}

}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchMute( BYTE nSwitch, bool bDown )
{
	if ( bDown && (m_bSwitches[MC_LOOP_ON_OFF] || m_bSwitches[MC_SCRUB]))
	{
		SONAR_MIXER_STRIP eInitialMixerStrip = m_cState.GetMixerStrip();

		if ( nSwitch >= MC_MUTE_1 && nSwitch <= MC_MUTE_8 )
		{
			SetPluginNumOffset( nSwitch - MC_MUTE_1 );
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchRecArm( BYTE nSwitch, bool bDown )
{
	if ( bDown && (m_bSwitches[MC_LOOP_ON_OFF] || m_bSwitches[MC_SCRUB]))
	{
		SONAR_MIXER_STRIP eInitialMixerStrip = m_cState.GetMixerStrip();

		if ( nSwitch >= MC_REC_ARM_1 && nSwitch <= MC_REC_ARM_8 )
		{
			SetParamNumOffset( NUM_MAIN_CHANNELS * (nSwitch - MC_REC_ARM_1 ));
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::OnSwitchPlugin(bool bSwitchToSynthRack )
{
	// Switch to MIX_STRIP_RACK if modifier M1 is pressed
	if ( bSwitchToSynthRack )
	{
		SavePreSynthRackAssignments();
		OnSelectMixerStrip( MIX_STRIP_RACK );
		m_cState.EnableModifier( MCS_MODIFIER_EDIT );
		m_cState.SetAssignmentMode( MCS_ASSIGNMENT_CHANNEL_STRIP );
	}

	// Switch to plugin assignment regardless - MIX_STRIP_RACK needs this too
	OnSelectAssignment( MCS_ASSIGNMENT_PLUGIN );
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::SavePreSynthRackAssignments()
{
	m_cState.SavePreSynthRackAssignments();
}

/////////////////////////////////////////////////////////////////////////////

void CMackieControlMaster::RestorePreSynthRackAssignments()
{
	OnSelectAssignment( m_cState.GetPreSynthRackAssignment() );

	if ( m_cState.GetPreSynthRackEditMode() )
	{
		m_cState.EnableModifier( MCS_MODIFIER_EDIT );
		SetLED( MC_EDIT, 1, true );
	}
	else
	{
		m_cState.DisableModifier( MCS_MODIFIER_EDIT );
		SetLED( MC_EDIT, 0, true );
	}

	m_dwMasterUpdateCount++;

	m_cState.SetAssignmentMode( m_cState.GetPreSynthRackAssignmentMode() );
}

/////////////////////////////////////////////////////////////////////////////

bool CMackieControlMaster::OnHuiSwitch(BYTE bD1, BYTE bD2)
{
	CCriticalSectionAuto csa(m_cState.GetCS());

	bool bDown = (bD2 == 0x7F);
	bool useKeypad = m_cState.GetHUIKeyPadControlsKeyPad();

	switch (bD1)
	{
		case HUI_KEYPAD_0:			if (bDown) FakeKeyPress(false, false, false, useKeypad ? VK_NUMPAD0 : '0');
		case HUI_KEYPAD_1:			if (bDown) FakeKeyPress(false, false, false, useKeypad ? VK_NUMPAD1 : '1');
		case HUI_KEYPAD_2:			if (bDown) FakeKeyPress(false, false, false, useKeypad ? VK_NUMPAD2 : '2');
		case HUI_KEYPAD_3:			if (bDown) FakeKeyPress(false, false, false, useKeypad ? VK_NUMPAD3 : '3');
		case HUI_KEYPAD_4:			if (bDown) FakeKeyPress(false, false, false, useKeypad ? VK_NUMPAD4 : '4');
		case HUI_KEYPAD_5:			if (bDown) FakeKeyPress(false, false, false, useKeypad ? VK_NUMPAD5 : '5');
		case HUI_KEYPAD_6:			if (bDown) FakeKeyPress(false, false, false, useKeypad ? VK_NUMPAD6 : '6'); 
		case HUI_KEYPAD_7:			if (bDown) FakeKeyPress(false, false, false, useKeypad ? VK_NUMPAD7 : '7');
		case HUI_KEYPAD_8:			if (bDown) FakeKeyPress(false, false, false, useKeypad ? VK_NUMPAD8 : '8');
		case HUI_KEYPAD_9:			if (bDown) FakeKeyPress(false, false, false, useKeypad ? VK_NUMPAD9 : '9');
		case HUI_KEYPAD_ENTER:		if (bDown) FakeKeyPress(false, false, false, VK_RETURN);
		case HUI_KEYPAD_PLUS:		if (bDown) FakeKeyPress(false, false, false, useKeypad ? VK_ADD : '+');
		case HUI_KEYPAD_MINUS:		if (bDown) FakeKeyPress(false, false, false, useKeypad ? VK_SUBTRACT : '-');
		case HUI_KEYPAD_EQUALS:		if (bDown) FakeKeyPress(false, false, false, '=');
		case HUI_KEYPAD_FWDSLASH:	if (bDown) FakeKeyPress(false, false, false, useKeypad ? VK_DIVIDE : '/');
		case HUI_KEYPAD_ASTERISK:	if (bDown) FakeKeyPress(false, false, false, useKeypad ? VK_MULTIPLY : '*');
		case HUI_KEYPAD_DOT:		if (bDown) FakeKeyPress(false, false, false, useKeypad ? VK_SEPARATOR : '.');
	}

	// Keep a note of the last button pressed
	m_bLastButtonPressed = bD1;

	return true;
}

