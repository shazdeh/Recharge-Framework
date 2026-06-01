Scriptname RechargeFramework_Utils Hidden

int[] Function GetVersion() Global Native

; returns true if akItem uses the framework for recharging
; akItem must be either a Weapon, or an Enchantment form
Bool Function IsRechargable(Form akItem) Global Native

; returns the amount of charge akItem provides to whatever item is currently being recharged
Int Function GetItemCharge(Form akItem) Global Native

; check if equipped weapon on akActor uses the framework
Bool Function EquippedWeaponIsRechargeable(Actor akActor, Bool abLeftHand = False) Global
    Weapon equipped = akActor.GetEquippedWeapon(abLeftHand)
    If equipped && IsRechargable(equipped)
        Return True
    EndIf

    Enchantment equippedEnchant = WornObject.GetEnchantment(akActor, (!abLeftHand) as Int, 0)
    If equippedEnchant && IsRechargable(equippedEnchant)
        Return True
    EndIf
EndFunction