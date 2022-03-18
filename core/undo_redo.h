/*************************************************************************/
/*  undo_redo.h                                                          */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2021 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2021 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef UNDO_REDO_H
#define UNDO_REDO_H

#include "core/object.h"
#include "core/resource.h"

class UndoRedo : public Object {

	GDCLASS(UndoRedo, Object);
	OBJ_SAVE_TYPE(UndoRedo);

public:
	enum MergeMode {
		MERGE_DISABLE,
		MERGE_ENDS,
		MERGE_ALL
	};

	enum OperationType {
		OPERATION_TYPE_METHOD,
		OPERATION_TYPE_PROPERTY,
		OPERATION_TYPE_REFERENCE
	};

	typedef void (*CommitNotifyCallback)(void *p_ud, const String &p_name);
	Variant _add_do_method(const Variant **p_args, int p_argcount, Variant::CallError &r_error);
	Variant _add_undo_method(const Variant **p_args, int p_argcount, Variant::CallError &r_error);

	typedef void (*MethodNotifyCallback)(void *p_ud, Object *p_base, const StringName &p_name, VARIANT_ARG_DECLARE);
	typedef void (*PropertyNotifyCallback)(void *p_ud, Object *p_base, const StringName &p_property, const Variant &p_value);

private:
	struct Operation {

		UndoRedo::OperationType type;
		Ref<Resource> resref;
		ObjectID object;
		String name;
		Variant args[VARIANT_ARG_MAX];

		operator Dictionary() const {
			Dictionary d;
			d["name"] = name;
			d["type"] = type;
			d["resref"] = resref.operator Variant();
			d["object"] = ObjectDB::get_instance(object);

			Array args_array;
			for (int i = 0; i < VARIANT_ARG_MAX; i++) {
				if (args[i].get_type() == Variant::NIL) {
					break;
				}
				args_array.append(args[i]);
			}
			d["args"] = args_array;

			return d;
		}
	};

	struct Action {
		String name;
		List<Operation> do_ops;
		List<Operation> undo_ops;
		uint64_t last_tick;

		operator Dictionary() const {
			Dictionary d;
			d["name"] = name;

			Array do_array;
			for (const List<Operation>::Element *E = do_ops.front(); E; E = E->next()) {
				do_array.append(E->get().operator Dictionary());
			}
			d["redo_operations"] = do_array;

			Array undo_array;
			for (const List<Operation>::Element *E = undo_ops.front(); E; E = E->next()) {
				undo_array.append(E->get().operator Dictionary());
			}
			d["undo_operations"] = undo_array;

			d["time"] = last_tick;
			return d;
		}
	};

	Vector<Action> actions;
	int current_action;
	int action_level;
	MergeMode merge_mode;
	bool merging;
	uint64_t version;

	void _pop_history_tail();
	void _process_operation_list(List<Operation>::Element *E);
	void _discard_redo();

	CommitNotifyCallback callback;
	void *callback_ud;
	void *method_callbck_ud;
	void *prop_callback_ud;

	MethodNotifyCallback method_callback;
	PropertyNotifyCallback property_callback;

	int committing;

protected:
	static void _bind_methods();

public:
	void create_action(const String &p_name = "", MergeMode p_mode = MERGE_DISABLE);

	void add_do_method(Object *p_object, const String &p_method, VARIANT_ARG_LIST);
	void add_undo_method(Object *p_object, const String &p_method, VARIANT_ARG_LIST);
	void add_do_property(Object *p_object, const String &p_property, const Variant &p_value);
	void add_undo_property(Object *p_object, const String &p_property, const Variant &p_value);
	void add_do_reference(Object *p_object);
	void add_undo_reference(Object *p_object);

	bool is_committing_action() const;
	void commit_action();

	bool redo();
	bool undo();
	String get_current_action_name() const;
	void clear_history(bool p_increase_version = true);

	bool has_undo();
	bool has_redo();

	uint64_t get_version() const;

	void set_commit_notify_callback(CommitNotifyCallback p_callback, void *p_ud);

	void set_method_notify_callback(MethodNotifyCallback p_method_callback, void *p_ud);
	void set_property_notify_callback(PropertyNotifyCallback p_property_callback, void *p_ud);

	Dictionary get_action(int p_action);
	Array get_all_actions();
	int get_current_action();
	int get_action_count();

	UndoRedo();
	~UndoRedo();
};

VARIANT_ENUM_CAST(UndoRedo::MergeMode);

#endif // UNDO_REDO_H
