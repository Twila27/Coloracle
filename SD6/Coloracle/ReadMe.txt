Assignment Name: Client-Server Game
Assignment Number: 06
Name: Benjamin Gibson
Class: Software Development VI
Instructor: Chris Forseth
	

//-----------------------------------------------------------------------------	
A6 Progress (below readme sections unchanged)
	- Wasn't able to get more than the host avatar syncing on screen after finally getting the "lobby" (no LAN broadcast/discovery => have to use commands to Join) to work.
	- Time focused on addressing the new Object Sync system and host-client game-side net architecture, fixing a previous problem with heartbeats not sending, and a night on art.
	- Problems with players moving (WASD) weirdly depending on the framerate despite the use of a variable delta time (testing with NetSimLag off).
	- Still has that particle misbind issue if you use Spacebar while moving around to drop explosions.
	
A5 Known Issues / Unimplemented Features
	- Instead of the last error paradigm mentioned in the spec for checking async failures, as I hadn't yet understood it, I used a Stopwatch event callback on timeout in NetSession.
	- A struct exists for in-order channel within NetConnection, but no container or further handling has been implemented for multiple of them.
	- Unreliable in-order traffic has not been implemented.
	- Singleton NetSession has been altogether removed, now just has NetSession* m_gameSession inside TheGame class. Game-side netcode primarily exists in TheGameNetworking.cpp.
		- So please let me know what else should probably be in the game side rather than engine side, if anything stands out during grading.
		
New A5 Console Commands
	NetSessionHost [username]
	NetSessionHostStartListening
	NetSessionHostStopListening
	NetSessionJoin <username> <hostIP> <port#>
	NetSessionPingsInOrder <IP> <port#> -- needs to have a connection to them first, either using A5 host/join or the A3 NetSessionCreateConnection command further below.
	
A5 Recommended Console Command Test Input (Like A4, hitting F1 will bring up NetSession debug info. Have the console down when toggling F1, or console eats the input.)
	1. On Host
		BroadcastCommand NetSessionStart
		NetSessionHost [username1]
		NetSessionHostStartListening //type "netsessionhosts" with no quotes, hit tab to autocomplete. Can wait to do this one until after the below to test joinDeny report.
	2. On Client
		NetSessionJoin <username2> <hostIP> <port#=4334>
		NetSessionPingsInOrder <connectedIP=host's> <port#=4334> //Normally, would use CreateConnection/DestroyConnection, but host/join achieve it with less hassle.
			//Use PgUp/PgDown to scroll debug output. 
			//Command sends 8 pings in-order, so first you should see than 8+ msg packet was observed by the receiving side.
			//A second 8+ msg packet was getting received after the first's messages report their reliableIDs and sequenceIDs, but !HasReceivedReliable(msg.reliableID) kicks it out.


//-----------------------------------------------------------------------------
Build Notes
	.exe will crash if it cannot find these directories and their files from its working directory $(SolutionDir)Run_$(PlatformName)/ under Project Properties:
	Data/Fonts
	Data/Models
	Data/Images
	Data/Shaders
	Data/XML/SpriteResources/Test.Sprites.xml
	fmod.dll, fmodstudio.dll


//-----------------------------------------------------------------------------
A4 Spec Unimplemented Features
	- Still need to add an assert when we try to confirm a reliable id outside of our max active range.
	- Was not able to get the series of pings sent as reliable traffic in, only the exploding game event. This may mean having 2+ reliables in a packet is untested, despite my spamming the explosions like crazy.
	- A2 Feedback
		- Unlock/lock window focus and hide/show cursor currently misbehaving, so on showing the console with tilde will unlock and show (that is, after the program has started, it seems to not trigger an unlock because the window regains focus after the console is shown automatically by the remote command service's auto-connect).
		- Pressing up still iterates through more than only console commands, need to think of a way to different the two using .find() that still works for BroadcastCommand (and also make BroadcastCommand work with auto-complete).
		- Session Creation/Registration/etc should exist in game code (NetSession is Engine - using it is game), but still exist in the engine project for A4.
		- Need to factor out repeated code between SendMessageDirect, SendMessagesDirect, as well as other uses of NetPacket into a FinalizePacket().
		
A3 Spec Unimplemented Features
	- Connections can set their own tick rates (useful for slowing down traffic to a congested channel).
	- PacketChannel does all socket/delay operations on a separate dedicated thread (helps as it affords blocking sockets, to know exactly when traffic arrives, or data compression).

A2 Spec Unimplemented Features
	- NetSession will auto choose an ID if one isn’t provided. (Wasn't sure what that one referred to--the protocol?)
	

//-----------------------------------------------------------------------------
A4 Known Issues & Open Questions
	- Because it takes place on the engine-side, DestroyConnection as is called during disconnection of a timed-out index does not remove the corresponding game dummy object owned by that connection. I'm unsure whether this Update loop (like creation/registration) should be moved game-side or not, though.
	- Not sure whether NetSimLag should be factored into lastRecvTime debug display, so it currently won't for example slow down to > the bad/timeout limits if you use NetSimLag (though NetSimLag has been tested to accurately lag the explosion reliable event, it just doesn't factor into the debug window).
	- Checking to ensure that our prevAcksAsbitfield offset value doesn't exceed 16 (per our bitfield being 16 bits long) via assertion was firing off far too sensitively and were thus commented out, not sure what consequences this may have.


A3 Known Issues/Changes
	- Because the other connections currently continue to send net traffic if you're on a breakpoint, can quickly cause PacketChannel's object pool to overflow.
		- Next week will implement a hack to timeout said traffic to a connection that's stopped responding for some set duration.
		- Just note that this will break the normal timeout logic, so said fix needs to be off by default, only on when breakpoint/debugging is necessary.
	- NetSimLoss was changed from only taking a single float percentile to also taking a range.
	- Carrot colors are randomized, but not sent across connections.
	- Because text render is slow, and this assignment yields a lot of it, slowdown while typing commands is recommended to be reduced via ClearLog command.


A2 Known Issues
	- Crashing can occur with multiple program instances on exit when the same log file is attempted to be used by both, will report (stream != nullptr) on abort in fwrite.cpp.
	- Since the current ping/pong message types do not really afford usage of SendMessagesDirect(), the support for multiple messages in a packet is present, but not demonstrable.

A1 Known Issues
	- A server's not able to see whether a client has left its server, to know to terminate.
	- Command echo cannot be toggled off.
	

//-----------------------------------------------------------------------------
A4 Most Pertinent Info / Controls
	- Pressing F1 (with console prompt closed) will bring up debug info on screen.
		- Networking debug renders only after NetSessionStart runs.
		- BAD: because of individual text render, program slows as more debug/console text builds (ClearLog helps only somewhat).
	- A NetSimLoss command with argument 1.0 may be used to test marking connections bad (game entity should stop moving on the other screen that has the 100% packet drop after 5secs, since only heartbeats will be sent) and timing them out (after 15s, should vanish from debug window--also appears in console, but I don't open it since it's likely the debug window suffices as is).
	- Hitting spacebar after creating game entities via NetSessionCreateConnection will fire off an explosion game event, which tests reliable traffic.
		
A3 Recommended Console Command Set (after opening 2 program instances, programs will autoconnect the remote command service)
	BroadcastCommand NetSessionStart
	BroadcastCommand NetSessionCreateConnection 0 <above cmd's printed-out ipStr> <above cmd's printed-out port#> c0
	BroadcastCommand NetSessionCreateConnection 1 <above cmd's printed-out ipStr> <above cmd's printed-out port#> c1
	-- and then any simulated channel lag/loss here, the defaults set by PacketChannel's constructor equate to:
	[NetSimLag 100 150]
	[NetSimLoss .01 .05]


//-----------------------------------------------------------------------------
New Convenience Console Commands
	FreeMouse (e.g. for easier switching between program instances)
	ToggleMouse (i.e. showing/hiding cursor, either bugged or repeatedly overridden at moment)
	
New A4 Console Commands
	None!

New A3 Console Commands
	BroadcastCommand 			<commandStr>				-- note this does stop autocomplete from working :(
	NetSessionCreateConnection 	<0-64 connIndex> <ipStr> <port#> <guidStr>
	NetSessionDestroyConnection	<0-64 connIndex>
	NetSimLag 					<min ms> [max #ms]			-- note this is in addition to base network latency!
	NetSimLoss 					<float lossPercentage01>	-- note this is in addition to base network loss!

A2 Console Commands
	NetSessionStart
	NetSessionStop
	NetSessionPing 				<ipStr> <port#> [messageStr] 	(uses SendMessageDirect)
	NetSessionPingPingPingPing 	<ipStr> <port#> [messageStr]	(test of SendMessagesDirect)

A1 Console Commands
	NetListTcpAddresses <port#>
	CommandServerHost
	CommandServerStop
	CommandServerJoin 	<host ipStr>
	CommandServerLeave
	CommandServerInfo
	SendCommand 		<commandStr>


//-----------------------------------------------------------------------------
Keyboard Controls
		WASD: Move Owned DummyNetObject (i.e. your carrot sprite, only created upon CreateConnection command use)
		Hold Shift: Flycam Speed-up (x8, editable as FLYCAM_SPEED_SCALAR in GameCommon.hpp)
		ESC: Exit
		F1: Toggle Debug Info (O-key to show/hide debug axes, after using ToggleActiveCameraMode and ToggleShowing3D to access controls for and unhide 3D layer, respectively.)

Console Mode Controls
	Tilde: Show/Hide Dev Console
	Tab: Auto-complete (use with blank input to cycle through all commands)
	Enter: Execute/Close
	Escape: Clear/Close
	PageUp/PageDown/Scroll: Scroll Output (necessary for 'help' command, some are pushed off-screen now)
	Left/Right: Move Caret
	Home/End: Move Caret
	Delete: Delete
	Up/Down: Command History
	
	
//-----------------------------------------------------------------------------
Resource Attributions
	Fonts		BMFont Software
	Input Font	http://input.fontbureau.com/
	Images		Chris Forseth, Dan Cook
	Models		Chris Forseth, Unity Technologies Japan, Komal K.