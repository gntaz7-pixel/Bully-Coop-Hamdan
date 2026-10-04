Bully Co-op Hamdan v0.6 - REAL IN-GAME POSITION NETWORK TEST (NO SECOND CHARACTER YET)
===============================================================================
تم إثبات قراءة موقع جيمي بنجاح في v0.5 من اللوق الذي أرسله حمدان.
هذه النسخة تضيف إرسال واستقبال إحداثيات اللاعبين عبر UDP بين جهازين داخل اللعبة نفسها.
هذه تجربة قراءة/شبكة فقط؛ لا تنشئ شخصية ثانية، ولا تزامن المهمات أو القتال.

طريقة التجربة:
1. ارفع محتويات هذا الأرشيف إلى جذر مستودع GitHub الخاص بـ Bully.
2. افتح Actions > Build Bully Co-op Network Position Probe v0.6 (x86) ثم Run workflow.
3. حمّل dinput8.dll المبني؛ لا تستخدم المكتبة قبل نجاح البناء.
4. اغلق Bully عند الجهازين، واحفظ نسخة احتياطية من أي dinput8.dll أصلي.
5. ضع dinput8.dll الجديد بجانب Bully.exe عندك وعند خويك.
6. عند الهوست: انسخ BullyCoop_HOST.ini باسم BullyCoop.ini إلى جانب Bully.exe.
7. عند الكلاينت: انسخ BullyCoop_GUEST.ini باسم BullyCoop.ini، ثم عدل HostAddress إلى
   عنوان IPv4 الخاص بالهوست داخل شبكة محلية أو Radmin VPN/ZeroTier.
8. تأكد أن Port=7791 و SessionCode متطابقان عند الاثنين؛ يفضل تغيير SessionCode
   إلى رقم مشترك خاص بكم (ستة أرقام على الأقل). الرمز ليس تشفيرا ولا حماية قوية.
9. اسمح لبرنامج Bully باتصالات UDP على المنفذ 7791 في جدار حماية ويندوز عند الهوست.
10. شغلوا Bully وادخلوا العالم بشخصية جيمي وامشوا بجهازين.
11. افتح BullyCoop_bridge.log عند كل جهاز وابحث عن:
     BullyCoop net v0.6: HOST UDP ready (read-only position exchange)
     BullyCoop net v0.6: GUEST UDP ready (read-only position exchange)
     BullyCoop net: first remote player position RECEIVED
     BullyCoop remote player: x=... y=... z=... age_ms=...
   هذه هي إحداثيات لاعب الطرف الثاني في جهازك؛ يجب أن تتغير عندما يمشي الطرف الثاني.

ملاحظات مهمة:
- هذا الملف لا يدعم اللعب الجماعي داخل العالم بعد؛ يتبادل فقط إحداثيات X/Y/Z.
- لا تستخدمه على شبكة عامة/إنترنت مفتوح: بروتوكول الاختبار غير مشفر، مصمم لشبكة موثوقة.
- كل نسخة Bully يجب أن تطابق ملف Bully.exe الخاص بحمدان في نسخة v0.5، وإلا يتوقف الفحص.
- إذا لم يوجد BullyCoop.ini أو Enabled=0 فالشبكة معطلة؛ تبقى قراءة الموقع فقط.
- هذا الكود لم يبن ولم يختبر داخل اللعبة بعد؛ نجاح تجربة v0.5 لا يضمن نجاح v0.6.
- لا تستخدم dinput8.dll الخاص بـ VCCoop مع Bully؛ هذا مشروع منفصل.
