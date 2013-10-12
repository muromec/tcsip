
lobj += rtp/rtp_io
lobj += rtp/rtp_opus
lobj += rtp/rtp_pcmu
lobj += rtp/rtp_speex

lobj += tcsip/tcmedia
lobj += tcsip/tcsipuser
lobj += tcsip/tcsipreg
lobj += tcsip/tcsipcall
lobj += tcsip/tcuplinks
lobj += tcsip/tcsip

lobj += ipc/tcipc
lobj += ipc/tcreport

lobj += api/http
lobj += api/login
lobj += api/login_phone

lobj += x509/x509util
lobj += x509/x509gen

lobj += util/platpath

lobj += g711/g711

lobj += rehttp/http
lobj += rehttp/auth

lobj-$(linux) += sound/linux/asound
lobj-$(apple) += sound/apple/sound
lobj-$(apple) += sound/apple/sound_utils
lobj-$(android) += sound/android/opensl_io

lobj += store/sqlite3
lobj += store/history
lobj += store/contacts
lobj += store/store

lobj += jitter/ajitter

lobj += $(lobj-y)
