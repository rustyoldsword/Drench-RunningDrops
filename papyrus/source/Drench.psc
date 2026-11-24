Scriptname Drench Hidden
{Drench - Wet Skin Framework. Global API and console commands. Wetness, the wet look, SexLab/OStim handling
 and saved data live in Drench.dll; the DrenchBridge quest script only forwards mod events.}

; ------------------------------------------------------------------
; Settings
; ------------------------------------------------------------------
; Re-reads Data\SKSE\Plugins\Drench.ini.
Function ReloadSettings() Global Native

Bool Function IsSexLabAvailable() Global Native
Bool Function IsOStimAvailable() Global Native

; ------------------------------------------------------------------
; SexLab P+ bridge (called by DrenchBridge)
; ------------------------------------------------------------------
; aiPhase: 0 scene start / actors joined, 1 stage update, 2 scene end / actors leaving
Function SexLabUpdate(Form akThread, Actor[] akActors, Float[] afEnjoyment, Float[] afPain, Int aiPhase) Global Native
Function SexLabOrgasm(Actor akActor) Global Native

; ------------------------------------------------------------------
; Actors
; ------------------------------------------------------------------
; Manually started effects never stop by themselves (the player's is controlled by the menu).
Bool Function StartEffect(Actor akActor) Global Native
Function StopEffect(Actor akActor) Global Native
Bool Function IsActive(Actor akActor) Global Native
Float Function GetWetness(Actor akActor) Global Native
Function SetWetness(Actor akActor, Float afWetness) Global Native
; Maximum wetness from the ini ([Wetness] fWetnessCap). Other mods scale their own wetness range against it.
Float Function GetWetnessCap() Global Native
; afWetness < 0, afSpecular <= 0 and afGlossiness <= 0 release the forced value
Function ForceValues(Actor akActor, Float afWetness, Float afSpecular, Float afGlossiness) Global Native
String Function GetStatus(Actor akActor) Global Native
Function PrintStatus(Actor akActor) Global Native
; Status plus every skin geometry: live shader values, textures, and what Drench wrote on it.
Function Report(Actor akActor) Global Native
; Removes the RaceMenu overrides Wet Function Redux, WetFunction NG or Dewpoint 1.0 left on the actor
; (every skin specular/glossiness override included) and rebuilds the 3D. Drench itself stores nothing there.
Function CleanActor(Actor akActor) Global Native
Function StopAll(Bool abClearData) Global Native

; Per character, for every save and new game (Data\SKSE\Plugins\Drench_Actors.ini):
; "normal", "shine" (only the shine changes, the skin keeps its own maps) or "excluded" (never wet).
; Applies to every reference of the character. False for an actor that has no lasting base record.
Bool Function SetActorMode(Actor akActor, String asMode) Global Native
String Function GetActorMode(Actor akActor) Global Native

; akActor has just been washed: soaks the actor ([Bathing] in the ini). Bathing in Skyrim sends BiS_WashActorFinish.
Function Bathed(Actor akActor) Global Native
; Brings every loaded actor to its look again soon. Drench notices rebuilt skin on its own on the next update;
; this only shortens the wait after another mod rebuilt actor 3D.
Function RefreshVisuals() Global Native

Actor Function Target() Global
	Actor target = Game.GetCurrentCrosshairRef() as Actor
	If target == None
		target = Game.GetPlayer()
	EndIf
	Return target
EndFunction

; ------------------------------------------------------------------
; Console (Custom Console): drench status|start|stop|soak|dry|release|report|clean|reload|stopall|shine|exclude|normal
; Acts on the actor under the crosshair, or the player.
; ------------------------------------------------------------------
Function ConsoleStatus() Global
	PrintStatus(Target())
EndFunction

Function ConsoleStart() Global
	Actor target = Target()
	StartEffect(target)
	PrintStatus(target)
EndFunction

Function ConsoleStop() Global
	StopEffect(Target())
EndFunction

Function ConsoleSoak() Global
	Actor target = Target()
	StartEffect(target)
	SetWetness(target, 1000.0)
EndFunction

Function ConsoleDry() Global
	SetWetness(Target(), 0.0)
EndFunction

Function ConsoleRelease() Global
	ForceValues(Target(), -1.0, 0.0, 0.0)
EndFunction

Function ConsoleReport() Global
	Report(Target())
EndFunction

Function ConsoleClean() Global
	Actor target = Target()
	CleanActor(target)
	If target != Game.GetPlayer()
		CleanActor(Game.GetPlayer())
	EndIf
EndFunction

Function ConsoleReload() Global
	ReloadSettings()
EndFunction

Function ConsoleStopAll() Global
	StopAll(False)
EndFunction

Function ConsoleShine() Global
	SetActorMode(Target(), "shine")
EndFunction

Function ConsoleExclude() Global
	SetActorMode(Target(), "excluded")
EndFunction

Function ConsoleNormal() Global
	SetActorMode(Target(), "normal")
EndFunction
