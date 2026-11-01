
#use 4 space indentation!

def _original_doors(event, filename):
    # Optional locally built mod. Ordinary Temple+/Co8 installs have no adapter.
    try:
        import bg2toee_lifecycle
    except ImportError as exc:
        if str(exc) in ('No module named bg2toee_lifecycle', "No module named 'bg2toee_lifecycle'"):
            return
        raise
    getattr(bg2toee_lifecycle, event)(filename)

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
