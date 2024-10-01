# Porting

I have given up on my original gtk upgrade plan of ```C gtk3 -> C gtk4 -> CPP gtkmm4```.

This is is due too, in order

- GMenu
- Signal/Event handling changes
- How dialogs get run, aka 'presented' by the main Gtk::Window
- GtkIconView and GtkTreeModel being deprecated for GtkGridView, aka rewriting the file browser
- Other stuff that I have repressed any memory of

Current gtkmm4 porting plan is

- rewrite every dialog as a standalone gtkmm4 program
- rewrite the main Gtk::Window and file browser
- Hope I finish step 2

This is either genius or borderline retarded.

## TODO

- MessageDialog and ErrorDialog should use Gtk::AlertDialog but that type cannot be launched standalone. These two dialogs should be rewritten to use Gtk::AlertDialog when the main Gtk4 rewrite is done.
