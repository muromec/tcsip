
lobj += rtp_io
lobj += rtp_opus
lobj += rtp_pcmu
lobj += rtp_speex

lobj += tcmedia
lobj += tcsipuser
lobj += tcsipreg
lobj += tcsipcall
lobj += tcuplinks
lobj += tcsip
lobj += tcipc
lobj += tcreport
lobj += x509util
lobj += x509gen
lobj += platpath

lobj-$(linux) += sound/linux/asound
lobj-$(apple) += sound/apple/sound
lobj-$(apple) += sound/apple/sound_utils
lobj-$(android) += sound/android/opensl_io

lobj += store/sqlite3
lobj += store/history
lobj += store/contacts

lobj += ajitter

lobj += $(lobj-y)
