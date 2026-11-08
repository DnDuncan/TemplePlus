
#use 4 space indentation!

def _original_doors(event, filename):
    # Optional locally built mod. Ordinary Temple+/Co8 installs have no adapter.
    try:
        import bg2toee_lifecycle
    except ImportError as exc:
        if str(exc) in ('No module named bg2toee_lifecycle', "No module named 'bg2toee_lifecycle'"):
            return
        raise
    handler = getattr(bg2toee_lifecycle, event, None)
    if handler is None and event in ('map_ready', 'door_trap', 'journal_key', 'source_container_open', 'scene_pulse', 'scene_turn', 'scene_begin'):
        return  # older adapter without the optional event
    return handler(filename)

def pre_load(filename):
    _original_doors('pre_load', filename)

def save(filename):
    try:
        _original_doors('save', filename)
    except Exception as exc:
        print('Original door save failed: ' + str(exc))
        return False
    return True

def load(filename):
    _original_doors('load', filename)

def map_ready(map_id):
    # Called by the engine once a map and its party are fully present
    # (after teleport or savegame load). Optional bg2toee adapter only.
    try:
        _original_doors('map_ready', map_id)
    except Exception as exc:
        print('bg2toee map_ready failed: ' + str(exc))

def journal_key():
    # J key in the game (bg2toee: BG journal). True when the key was used.
    try:
        return bool(_original_doors('journal_key', None))
    except Exception as exc:
        print('bg2toee journal_key failed: ' + str(exc))
        return False

def source_container_open(container, actor):
    try:
        _original_doors('source_container_open', (container, actor))
    except Exception as exc:
        print('bg2toee source_container_open failed: ' + str(exc))


def door_trap(map_id, door_id, actor):
    # A source door trap was sprung by an engine context action.
    _original_doors('door_trap', (map_id, door_id, actor))

# Co8 python save hook - executes after the archive is done
# save hook (default - Co8 hook; will silently fail for vanilla; feel free to change for your own custom mod!)
def post_save(filename):
    try:
        import _co8init
        print("imported Co8Init inside templeplus package")
        _co8init.save(filename)
        print("Co8 Save hook successful")
    except:
        print("Co8 Save hook failed\n")

# save hook (default - Co8 hook; will silently fail for vanilla; feel free to change for your own custom mod!)
def post_load(filename):
    try:
        import _co8init
        _co8init.load(filename)
    except:
        print("Co8 Load hook failed\n")

def scene_pulse(now):
    try:
        _original_doors('scene_pulse',now)
    except Exception as exc:
        print('bg2toee scene_pulse failed: '+str(exc))

def scene_turn(round_number,index,initiative):
    try:
        _original_doors('scene_turn',(round_number,index,initiative))
    except Exception as exc:
        print('bg2toee scene_turn failed: '+str(exc))

def scene_begin(scene):
    try:
        _original_doors('scene_begin',scene)
    except Exception as exc:
        print('bg2toee scene_begin failed: '+str(exc))
