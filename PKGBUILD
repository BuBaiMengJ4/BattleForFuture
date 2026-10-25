pkgname=yourapp
pkgver=1.2.3
pkgrel=1
pkgdesc="Your application"
arch=('x86_64')
url="https://github.com/you/repo"
license=('MIT')
depends=('qt6-base' 'hicolor-icon-theme')
makedepends=('qt6-tools')
source=()

build() {
  cd /src
  mkdir -p build-arch
  cd build-arch
  qmake6 CONFIG+=release PREFIX=/usr ../YourProject.pro
  make -j"$(nproc)"
}

package() {
  install -Dm755 /src/build-arch/YourApp "$pkgdir/usr/bin/yourapp"
  install -Dm644 /src/resources/yourapp.desktop \
    "$pkgdir/usr/share/applications/yourapp.desktop"
  if [ -f /src/resources/yourapp.png ]; then
    install -Dm644 /src/resources/yourapp.png \
      "$pkgdir/usr/share/icons/hicolor/256x256/apps/yourapp.png"
  fi
}
