Scriptname DrenchBridge extends Quest
{Forwards to Drench.dll what only Papyrus can see: SexLab / SexLab P+ scene data and the mod events other
 mods send (Bathing in Skyrim, Afterglow). Configuration is native, through SKSE Menu Framework.}

Event OnInit()
	RegisterEvents()
EndEvent

Function RegisterEvents()
	UnregisterForAllModEvents()
	RegisterForModEvent("AnimationStart", "OnSexLabScene")
	RegisterForModEvent("ActorChangeStart", "OnSexLabScene")
	RegisterForModEvent("ActorChangeEnd", "OnSexLabScene")
	RegisterForModEvent("AnimationEnd", "OnSexLabScene")
	RegisterForModEvent("StageStart", "OnSexLabScene")
	RegisterForModEvent("SexLabOrgasm", "OnSexLabOrgasm")
	; Bathing in Skyrim: the wash is over -> wet; dirt re-applied to every actor after a load -> repaint
	RegisterForModEvent("BiS_WashActorFinish", "OnBiSWashActorFinish")
	RegisterForModEvent("BiS_UpdateActorsAll", "OnBiSUpdateActorsAll")
	; sent by mods written for WetFunction after they rebuilt a face
	RegisterForModEvent("WetFunction_ForceHeadRefresh", "OnRefresh")
	RegisterForModEvent("Drench_Refresh", "OnRefresh")
EndFunction

Event OnSexLabScene(String asEvent, String asThread, Float afValue, Form akSender)
	sslThreadController thread = akSender as sslThreadController
	If thread == None
		Return
	EndIf
	Int phase = 1
	If asEvent == "AnimationStart" || asEvent == "ActorChangeEnd"
		phase = 0
	ElseIf asEvent == "AnimationEnd" || asEvent == "ActorChangeStart"
		phase = 2
	EndIf
	Actor[] positions = thread.GetPositions()
	Int count = positions.Length
	Float[] enjoyment = Utility.CreateFloatArray(count)
	Float[] pain = Utility.CreateFloatArray(count)
	If phase == 1
		Int i = 0
		While i < count
			If positions[i]
				enjoyment[i] = thread.GetEnjoyment(positions[i]) as Float
				pain[i] = thread.GetPain(positions[i]) as Float
			EndIf
			i += 1
		EndWhile
	EndIf
	Drench.SexLabUpdate(akSender, positions, enjoyment, pain, phase)
EndEvent

Event OnSexLabOrgasm(Form akActor, Int aiEnjoyment, Int aiOrgasms)
	Drench.SexLabOrgasm(akActor as Actor)
EndEvent

Event OnBiSWashActorFinish(Form akBathingActor, Form akWashProp, Bool abUsingSoap)
	Actor bather = akBathingActor as Actor
	If bather
		Drench.Bathed(bather)
	EndIf
EndEvent

Event OnBiSUpdateActorsAll()
	Drench.RefreshVisuals()
EndEvent

Event OnRefresh(String asEvent, String asArg, Float afValue, Form akSender)
	Drench.RefreshVisuals()
EndEvent
