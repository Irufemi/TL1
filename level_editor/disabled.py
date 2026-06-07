import bpy

# オペレータ 無効オプションを追加する
class MYADDON_OT_add_disabled(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_add_disabled"
    bl_label = "無効オプション 追加"
    bl_description = "['disabled']カスタムプロパティを追加します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        # ['disabled']カスタムプロパティを追加
        context.object["disabled"] = True

        return {'FINISHED'}

# パネル 無効オプション
class OBJECT_PT_disabled(bpy.types.Panel):
    bl_idname = "OBJECT_PT_disabled"
    bl_label = "Disabled"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"

    def draw(self, context):
        # カスタムプロパティがなければ追加ボタンを表示
        if "disabled" in context.object:
            self.layout.prop(context.object, '["disabled"]', text="disabled")
        else:
            self.layout.operator(MYADDON_OT_add_disabled.bl_idname, text="Add Disabled")
