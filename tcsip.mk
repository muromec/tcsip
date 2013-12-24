
rtp-obj += rtp/rtp
rtp-obj += rtp/rtp_io
rtp-obj += rtp/rtp_opus
rtp-obj += rtp/rtp_pcmu
rtp-obj += rtp/rtp_speex
rtp-obj += g711/g711

lobj += tcsip/tcmedia
lobj += tcsip/tcsipuser
lobj += tcsip/tcsipreg
lobj += tcsip/tcsipcall
lobj += tcsip/tcuplinks
lobj += tcsip/tcmessage
lobj += tcsip/tcsip

lobj += ipc/tcipc
lobj += ipc/tcreport

lobj += api/http
lobj += api/login
lobj += api/login_phone
lobj += api/signup

lobj += x509/x509util
lobj += x509/x509gen

lobj += util/platpath
lobj += util/ctime


lobj += rehttp/http
lobj += rehttp/auth

sound-obj-$(linux) += sound/linux/asound
sound-obj-$(apple) += sound/apple/sound
sound-obj-$(apple) += sound/apple/sound_utils
sound-obj-$(android) += sound/android/opensl_io
sound-obj += sound/sound

sound-obj += jitter/ajitter

sound-obj += $(sound-obj-y)

lobj += store/sqlite3
lobj += store/history
lobj += store/contacts
lobj += store/store

lobj += $(lobj-y)
lobj += $(sound-obj)
lobj += $(rtp-obj)
