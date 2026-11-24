Scriptname DrenchPlayerAlias extends ReferenceAlias
{Re-registers the event bridge after loading a save.}

Event OnPlayerLoadGame()
	DrenchBridge bridge = GetOwningQuest() as DrenchBridge
	If bridge
		bridge.RegisterEvents()
	EndIf
EndEvent
