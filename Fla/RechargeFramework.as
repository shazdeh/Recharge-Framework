import skse;
import skyui.defines.Inventory;

class RechargeFramework extends MovieClip {

    public static var instance;

    /* refs */
    public var Menu_mc:MovieClip;
    public var inventoryLists:MovieClip;
    public var itemList:MovieClip;
    public var itemCard:MovieClip;

    /* state */
    private var bRecharging:Boolean = false;

    function RechargeFramework() {
        RechargeFramework.instance = this;
    }

    function onLoad() {
        Menu_mc = _parent._parent.Menu_mc;
        inventoryLists = Menu_mc.inventoryLists;
        itemList = inventoryLists.itemList;
        itemCard = Menu_mc.itemCard;
        duckPunch();
    }

    function duckPunch() {
        Menu_mc.RF__AttemptChargeItem = Menu_mc.AttemptChargeItem;
        Menu_mc.AttemptChargeItem = AttemptChargeItem;

        itemCard.RF__onListItemPress = itemCard.onListItemPress;
        itemCard.onListItemPress = onListItemPress;
    }

    public function AttemptChargeItem(): Void {
        this = RechargeFramework.instance;
        bRecharging = false;
        if (inventoryLists.itemList.selectedIndex == -1) {
            return;
        }

        
        if (_root.RF_IsEnabled(inventoryLists.itemList.selectedIndex) === true) {
            if ( ! ( Menu_mc.shouldProcessItemsListInput(false) && itemCard.itemInfo.charge != undefined && itemCard.itemInfo.charge < 100 ) ) {
                return;
            }

            if (itemList.selectedEntry.equipState === Inventory.ES_NONE) {
                // we need the item equipped in order to update the ActorValue
                // @todo: remove this, update InventoryEntryData instead of modifying AVs
                Menu_mc.AttemptEquip();
            }
            bRecharging = true;
            onEnterFrame = function() { // wait 1 frame, give UI time to update from AttemptEquip()
                showList(_root.RF_RequestList(inventoryLists.itemList.selectedIndex, isLeftHand()));
                onEnterFrame = null;
            }
        } else {
            Menu_mc.RF__AttemptChargeItem();
        }
    }

    function onListItemPress(event: Object): Void {
        this = RechargeFramework.instance;
        if (bRecharging) {
            _root.RF_Charge(event.entry.id, isLeftHand());
            itemCard.HideListMenu();
            bRecharging = false;
        } else {
            itemCard.RF__onListItemPress(event);
        }
    }

    function isLeftHand() : Boolean {
        var equipState:Number = itemList.selectedEntry.equipState;
        return equipState === Inventory.ES_LEFT_EQUIPPED;
    }

    function showList(serializedData:String) {
        if (serializedData === '' || serializedData === undefined) return;

        var data:Array = serializedData.substr(0, serializedData.length - 2).split('||'),
            listItems:Array = new Array();
        var totalCharge:Number = parseInt(data[0]);
        for (var i = 1; i < data.length; i++) {
            var parts = data[i].split('//');
            var count:Number = parseInt(parts[2]),
                charge:Number = parseInt(parts[3]);
            listItems.push( {
                id : parts[0], // editor ID used to send and receive to the game
                text : parts[1] + " (" + count + ")",
                count : count,
                chargeAdded : charge === 0 ? totalCharge : Math.ceil((charge * 100) / totalCharge), // value of 0 means it fully recharges the item
                clipIndex : i + 1
            } );
        }

        itemCard.itemInfo = {
            currentCharge : itemCard.itemInfo.charge,
            type : Inventory.ICT_LIST,
            listItems : listItems
        };
    }
}