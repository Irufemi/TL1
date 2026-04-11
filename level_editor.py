import bpy

# ブレンダーに登録するアドオン情報
bl_info = {
    "name" : "LevelEditor",
    "author" : "Koichi Surhro",
    "version" : (1, 0),
    "blender" : (3, 3, 1),
    "location" : "",
    "description" : "LevelEditor",
    "warning" : "",
    # "support" : "TESTING",
    "wiki_url" : "",
    "tracker_url" : "",
    "category" : "Object"
}

# アドオン有効化時コールバック
def register():
    print("レベルエディタが有効化されました")
    
# アドオン無効化時コールバック
def unregister():
    print("レベルエディタが無効化されました")
    
# テスト実行用コード
if __name__ == "__main__":
    register()