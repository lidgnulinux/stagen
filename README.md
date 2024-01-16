## What's this ?

### Stage with some additional features.

Mirror of [stage-next](https://gitlab.com/lidgl/stage-next)

![Stagen with pseudo tile](/stagen.png "Stagen + pseudo tile")

The additional features are :

<details>
  <summary>Additional features.</summary>

- Tap to click support, for touchpad laptop users.
- Close window function, to close window via keybind.
- Window manipulation function via keybind.
    - Move window to any direction (left, right, up, down and center).
    - Resize window to any direction (left, right, up, down and half width and height of screen).
    - Maximize window.
    - Snap window to any direction (left, right, up, down and center).
    - Pseudo minimize.
    - Fullscreen window.
- launcher (wofi).
- Screencopy protokol to support screenshot.
- Primary selection protocol.
- Simple background color.
- Simple exclusive zone to place a bar / panel at right, toggleable. The only tested and working panel is lxqt-panel.
- Simple rule for a specific app_id / title, e.g for lxqt-panel.
- Screenshot via external tool (grim), for fullscreen screenshot and focused-window screenshot.
- Simple show desktop.
- Send to desktop, send all minimized windows back to desktop / surface.
- Cursor theme option, we can set the cursor theme using these envs, **XCURSOR_THEME** (cursor theme) and **XCURSOR_SIZE** (cursor size).
- Window maximizing and minimizing via decoration button. 
- Support for simple virtual keyboard and virtual pointer support, allowing us to use tools that need virtual keyboard and virtual pointer support (e.g. wtype, ydotool).
- Simple popup unconstrain, works on maximized window only.
- Workspace feature, optional.
- Half_width_height function, resizing window so the window has half width and height of screen.
- Support for compilation / build using meson.

</details>

By default, The available shortcut are :

<details>
  <summary>Shortcut.</summary>

- mod + enter : Open terminal (foot).
- mod + p : Open launcher (wofi).
- mod + h : maximize window vertically to left.
- mod + l : maximize window vertically to right.
- mod + f : maximize window.
- mod + shift + f : fullscreen window.
- mod + Shift + q : close / quit compositor.
- mod + q : close / kill window.
- mod + j : send window to the lowest / minimize.
- mod + k : send window to the highest position.
- mod + {left, right, up, down} : move window to left, right, up, down.
- mod + shift + {l, h, k, j} : resize window to left, right, up, down.
- mod + shift + {r, e, t, w} : move window to most left, right, up, down of the screen.
- mod + Tab : cycle window.
- mod + z : toggle exclusive zone.
- mod + w : screenshot for the focused-window.
- mod + shift + p : screenshot for fullscreen.
- mod + {0,1,2,..,9} : change focus to workspace 0 - 9.
- mod + shift + {0,1,2,..,9} : send window to desired `workspace`. 
- mod + shift + o : resize window so the window has half width and height of screen.

</details>

## Dependency.

TinyWay has dependency on wlroots library, check on wlroots [0.17.1](https://gitlab.freedesktop.org/wlroots/wlroots/-/releases/0.17.1). We will also make a branch which follow the latest development of wlroots.

## How to *build*, *install* and *run*.

These are steps to *build*, *install* and *run*.

<details>
  <summary>Click to expand.</summary>

1. Adjust the setting or configuration at config/config.h !
1. Adjust the keybind / shortcut at stage.c, check at handle_keybinding function (optional) !
1. Build with `make` !
1. Install with `make install` to install stagen !
1. To make screenshot work properly, run stage with this command !

    ```
    $ /path/to/stage > ~/.cache/tiny_info
    ```

1. Don't Forget to copy the screenshot script "misc/ss_window" to your $PATH !
1. We can run stage using custom cursor theme using these envs, **XCURSOR_THEME** (cursor theme) and **XCURSOR_SIZE** (cursor size).

    ```
    $ XCURSOR_THEME=your_cursor_theme XCURSOR_SIZE=cursor_size /path/to/stage > ~/.cache/tiny_info
    ```

1. Stage also support meson for compilation.

    ```
    $ meson setup build
    $ ninja -C build
    ```

</details>

