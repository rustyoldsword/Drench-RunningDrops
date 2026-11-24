Scriptname WetFunctionNG Hidden
{Compatibility layer of Drench - Wet Skin Framework for mods written against WetFunction NG: the same global
 functions, answered by Drench. Not WetFunction NG itself; install it only without WetFunction NG.}

Function ReloadSettings() Global
	Drench.ReloadSettings()
EndFunction

Bool Function IsSexLabAvailable() Global
	Return Drench.IsSexLabAvailable()
EndFunction

Bool Function IsOStimAvailable() Global
	Return Drench.IsOStimAvailable()
EndFunction

Function SexLabUpdate(Form akThread, Actor[] akActors, Float[] afEnjoyment, Float[] afPain, Int aiPhase) Global
	Drench.SexLabUpdate(akThread, akActors, afEnjoyment, afPain, aiPhase)
EndFunction

Function SexLabOrgasm(Actor akActor) Global
	Drench.SexLabOrgasm(akActor)
EndFunction

Bool Function StartEffect(Actor akActor) Global
	Return Drench.StartEffect(akActor)
EndFunction

Function StopEffect(Actor akActor) Global
	Drench.StopEffect(akActor)
EndFunction

Bool Function IsActive(Actor akActor) Global
	Return Drench.IsActive(akActor)
EndFunction

Float Function GetWetness(Actor akActor) Global
	Return Drench.GetWetness(akActor)
EndFunction

Function SetWetness(Actor akActor, Float afWetness) Global
	Drench.SetWetness(akActor, afWetness)
EndFunction

Float Function GetWetnessCap() Global
	Return Drench.GetWetnessCap()
EndFunction

Function ForceValues(Actor akActor, Float afWetness, Float afSpecular, Float afGlossiness) Global
	Drench.ForceValues(akActor, afWetness, afSpecular, afGlossiness)
EndFunction

String Function GetStatus(Actor akActor) Global
	Return Drench.GetStatus(akActor)
EndFunction

Function PrintStatus(Actor akActor) Global
	Drench.PrintStatus(akActor)
EndFunction

Function Report(Actor akActor) Global
	Drench.Report(akActor)
EndFunction

Function CleanActor(Actor akActor) Global
	Drench.CleanActor(akActor)
EndFunction

Function StopAll(Bool abClearData) Global
	Drench.StopAll(abClearData)
EndFunction

Function Bathed(Actor akActor) Global
	Drench.Bathed(akActor)
EndFunction

Function RefreshVisuals() Global
	Drench.RefreshVisuals()
EndFunction

Function McmStopAll() Global
	Drench.StopAll(False)
EndFunction

Function McmClearAll() Global
	Drench.StopAll(True)
EndFunction

Function McmCleanPlayer() Global
	Drench.CleanActor(Game.GetPlayer())
EndFunction
