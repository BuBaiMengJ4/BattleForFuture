pkgname=battleforfuture
pkgver=3.0.0
pkgrel=1
pkgdesc="BattleForFuture application"
arch=('x86_64')
url="https://github.com/BuBaiMengJ4/BattleForFuture"
license=('MIT')
depends=('qt6-base' 'hicolor-icon-theme')
source=()

# 删除 build() 函数，直接使用 CI 前面步骤已经编译好的二进制文件
package() {
  # 注意：/src 是 docker run 挂载的当前仓库目录
  install -Dm755 /src/build/BattleForFuture "$pkgdir/usr/bin/battleforfuture"
  
  if [ -f /src/resources/battleforfuture.desktop ]; then
    install -Dm644 /src/resources/battleforfuture.desktop "$pkgdir/usr/share/applications/battleforfuture.desktop"
  fi
  
  if [ -f /src/resources/battleforfuture.png ]; then
    install -Dm644 /src/resources/battleforfuture.png "$pkgdir/usr/share/icons/hicolor/256x256/apps/battleforfuture.png"
  fi
}
