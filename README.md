# Stage Wayland Compositor (Versi Lanjut / Next)

## Apa ini ?

Stage merupakan wayland compositor yang dikembangkan oleh [mdepx](https://github.com/mdepx) sebagai window manager harian, digunakan secara pribadi oleh beliau. Saya melakukan fork pada repositori [stage](https://github/mdepx/stage) dan menambahkan beberapa fungsi dan fitur. Beberapa di antaranya sebagai berikut :

- Fungsi `maximize` jendela secara vertikal untuk arah kiri dan kanan.
- Fungsi `killclient` untuk menutup / kill jendela yang aktif.
- Fungsi `quit` untuk menutup kompositor.
- Dukungan fungsi `tap-to-click` untuk pengguna touchpad pada laptop.
- Dukungan gaps sederhana ketika jendela di-maximize secara vertikal.
- Penyesuaian Makefile untuk distro debian dan turunannya.
- Beberapa konfigurasi sederhana pada config.h
- Toggle zona eksklusif / exclusive zone.
- `Resize` dan `move` jendela lewat keybind dengan step / ukuran yang ditentukan. 
- `Rule` untuk jendela tertentu. 
- Fungsi `snap to edge` ke pojok kanan, kiri, atas dan bawah.
- Fungsi `move to center ` untuk memindah jendela tepat ke tengah layar.
- Fungsi `half_width_height` untuk me-resize jendela menjadi setengah dari lebar & tinggi.
- Fungsi `cycle window` untuk berpindah fokus dari satu jendela ke jendela yg lain.
- konfigurasi untuk mengatur warna background sederhana.
- Tangkapan layar / _screenshot_ (melalui _tool_ eksternal grim), baik layar penuh / _fullscreen_, maupun jendela terfokus.
- `Sticky` sederhana untuk beberapa jendela (lxqt-panel & pcmanfm-qt), agar bisa tetap terlihat pada semua workspace.
- Fungsi `pseudo minimize` atau minimize palsu, mengirim jendela ke bawah background, sehingga tidak terlihat.
- `Minimize` & `maximize` jendela melalui tombol CSD (Client Side Decoration).
- Fitur `sendto_ws`, mengirim jendela dari satu workspace ke workspace yang lain.

## Dependensi

Dependensi utama :

- Wlroots, bisa diunduh dengan `git submodule init` & `git submodule update`, lalu tinggal build wlroots-nya.
- Terminal foot, bisa diganti pada file `stage.c`.

NB : grim (opsional), untuk fitur _screenshot_.

## Shortcut

Shortcut bisa diatur dan ditambahkan pada file `stage.c` di bagian fungsi `handle_keybinding`.

Secara default shortcut yang tersedia :

- mod + enter : membuka terminal (foot).
- mod + p : membuka launcher (wofi).
- mod + {0,1,2,....,9} : beralih ke workspace 0 sampai 9.
- mod + h : maximize jendela arah vertikal ke sebelah kiri.
- mod + l : maximize jendela arah vertikal ke sebelah kanan.
- mod + f : maximize jendela.
- mod + shift + q : keluar / quit kompositor.
- mod + q : menutup / `close` jendela.
- mod + j : mengirim jendela ke posisi paling bawah / minimize.
- mod + k : mengirim jendela ke posisi paling atas.
- mod + {left, right, up, down} : memindah / `move` jendela ke arah kiri, kanan, atas dan bawah.
- mod + + shift + {l, h, k, j} : `resize` ukuran jendela ke arah kanan, kiri, atas, dan bawah.
- mod + shift + {u, d} : maximize jendela arah horizontal ke sebelah bawah dan atas.
- mod + shift + {r, e, t, w} : snap jendela ke arah pinggir kanan, kiri, atas, dan bawah.
- mod + shift + c : memindah jendela tepat ke tengah layar.
- mod + shift + o : me-resize jendela menjadi setengah dari lebar & tinggi.
- mod + Tab : Cycle jendela / window.
- mod + z : toggle zona eksklusif.
- mod + w : screenshot jendela terfokus.
- mod + shift + p : screenshot layar penuh / _fullscreen_.
- mod + shift + {0,1,2,....,9} : mengirim jendela ke `workspace` yang dituju. 

## Cara *build*, *install* dan *run*.

Berikut adalah cara untuk *build*, menginstall dan *run* stage :

1. Sesuaikan pengaturan / konfigurasi di `config.h` !
1. Sesuaikan keybind / shortcut di `stage.c` pada bagian fungsi `handle_keybinding` (opsional) ! 
1. *Build* dengan perintah `make`, untuk pengguna debian dan turunannya bisa menggunakan perintah `make -f Makefile-debian`, pengguna alpine bisa menggunakan berkas `Makefile-alpine` !
1. *Install* dengan perintah `make install` ! stage akan terinstall di `$HOME/.local/bin/`.
1. Stage bisa langsung dijalankan dengan perintah `$HOME/.local/bin/stage`.
1. Agar fitur screenshot bisa digunakan, jalankan TinyWay dengan perintah berikut !

    ```
    $ $HOME/.local/bin/stage > ~/.cache/tiny_info
    ```

1. Jangan lupa untuk menyalin contoh skrip _screenshot_ "addons/ss_window" ke $PATH !

NB :

- Untuk mendapatkan informasi yang lebih detail ketika melakukan `debug`, kita bisa menambahkan opsi `-ggdb` saat melakukan kompilasi.

contoh :

```
	cc -ggdb ${CFLAGS} -DSTAGE_DEV ${LDFLAGS} stage.c -o stage

```

## Fitur selanjutnya ?

Fitur yang ingin kami terapkan :

- Memindah config keybind ke config.h
- Fix issue virtual input.
