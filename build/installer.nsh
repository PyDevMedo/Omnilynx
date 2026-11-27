!macro customUnInstall
  MessageBox MB_YESNO|MB_ICONQUESTION "Do you want to completely delete all Omnilynx mods, compiled files, compiler, and settings?$\r$\n$\r$\nهل تريد حذف جميع المودات والملفات المترجمة والإعدادات الخاصة بـ Omnilynx بالكامل؟" IDNO omnilynx_skip_cleanup
    RMDir /r "$INSTDIR\Mods"
    RMDir /r "$INSTDIR\Compiled"
    RMDir /r "$INSTDIR\Config"
    RMDir /r "$INSTDIR\Compiler"
    RMDir /r "$LOCALAPPDATA\Omnilynx"
    RMDir /r "C:\Omnilynx"
  omnilynx_skip_cleanup:
!macroend
