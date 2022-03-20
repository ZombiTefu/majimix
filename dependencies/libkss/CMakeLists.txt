message(STATUS "Fetching libkss...")

add_compile_definitions(EXCLUDE_DRIVER_ALL)

# Copyright-ed drivers
# --------------------
# It's probably OK to use these drivers but we have to mention it
# and it is necessary to inform or ask for the agreement of the authors beforehand


# kinrou5.h from kinrou5.bin by Keiichi Kuroda - kuroda@g-net.org
# ---------------------------------------------------------------
#
# add_compile_definitions(EXCLUDE_DRIVER_KINROU)  


# mgsdrv.h from mgsdrv.com - MGSDRV (c) Ain./Gigamix
# --------------------------------------------------
# nf_ban (Takashi Kobayashi)
# https://gigamix.hatenablog.com/entry/mgsdrv/#MGSDRV-%E3%81%A8%E3%81%AF
#
# MGSDRV is free software. Anyone can enjoy MGSDRV freely and without charge.
# MGSDRV was developed by Mr. Ain. Currently, our team (Gigamix) is maintaining it.1
# (MGSDRVはフリーソフトウェアです。どなたでも無償で自由にMGSDRVを楽しむことができます。
#  MGSDRVはAin.氏が開発しました。現在では当チーム(ギガミックス)がメンテナンスを行っております。1)
# 
# add_compile_definitions(EXCLUDE_DRIVER_MGSDRV)


# mkp (mpk106 & mpk103)
# ---------------------
# mpk106.h from mpk.bin by K-KAZ
# mpk103.h from mpk103.bin by K-KAZ
# -------------------------------------------------
# 　ＭＰＫ ver x.xx 使用  (C)Copyright  Ｋ－ＫＡＺ
#            （~~~~使用したＭＰＫのバージョン）
# -------------------------------------------------
#
# add_compile_definitions(EXCLUDE_DRIVER_MPK106)

# opx
# ---
# opx4kss.h from opx4kss.bin by Mikasen (OPLLDriver by Ring is embedded).
#
# add_compile_definitions(EXCLUDE_DRIVER_OPX4KSS)


# probably ok
# add_compile_definitions(EXCLUDE_DRIVER_MBR143)
 
FetchContent_MakeAvailable(libkss)