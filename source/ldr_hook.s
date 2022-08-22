@race init hooks
ansub_02062D44:
		push {r0-r3}
		bl TryLoadRaceRCM
		pop {r0-r3}
		mov r0, #3
		b 0x02062D48
		
@race deinit hooks
ansub_0206206C:
		b TryUnloadRaceRCM
		
@title init hook
trepl_020AD348:
		push {lr}
		bl TryLoadSceneRCM
		pop {lr}
		b unlock_hasUnlockedTrueTitleScreen
		
@title deinit hook
trepl_020AD330:
		push {lr}
		bl TryUnloadSceneRCM
		pop {lr}
		ldr r0, =gmenu_finish
		bx r0
		
@menu init hook
ansub_02039F94:
		push {r12, lr}
		bl TryLoadSceneRCM
		pop {r12, lr}
		bx r12
		
@logo, local multiplayer menu, result screen deinit hooks
ansub_02057D74:
ansub_0204B034:
ansub_02059BDC:
		push {r12, lr}
		bl TryUnloadSceneRCM
		pop {r12, lr}
		bx r12

@menu deinit hook
ansub_02039F84:
		bl TryUnloadSceneRCM
		pop {pc}
		
@logo init hooks
ansub_02057D98:
		b TryLoadSceneRCM
		
@records init hook
trepl_021113B2:
		push {r0, lr}
		bl TryLoadSceneRCM
		pop {r0, lr}
		b proc_getCurrent
		
@records deinit hook
trepl_02111390:
		push {lr}
		blx 0x02111585
		pop {lr}
		b TryLoadSceneRCM
		
@options init hook
trepl_0219E9E6:
		push {r0, lr}
		bl TryLoadSceneRCM
		pop {r0, lr}
		b proc_getCurrent
		
@options deinit hook
trepl_0219E9CC:
		push {lr}
		blx 0x02192F8D
		pop {lr}
		b TryUnloadSceneRCM
		
@local multiplayer init hook
ansub_0204B03C:
		push {lr}
		bl TryLoadSceneRCM
		b 0x0204B040
		
@result screen init hook
ansub_02059BE4:
		push {lr}
		bl TryLoadSceneRCM
		b 0x02059BE8

@global scene init hook (for WiFi menu)
arepl_02048978:
		push {r0, r1, lr}
		bl scproc_getCurrentScene
		cmp r0, #12
		bleq TryLoadSceneRCM
		pop {r0, r1, lr}
		bx r1

@global scene deinit hook (for WiFi menu)
arepl_020496D4:
		push {r1, lr}
		blx r1
		bl scproc_getCurrentScene
		cmp r0, #12
		bleq TryUnloadSceneRCM 
		pop {r1, lr}
		bx lr
		