Bully Co-op Hamdan - POSITION DIAGNOSTIC v0.5 (READ-ONLY)
==========================================================

اللوق السابق أثبت أن DLL يعمل، لكنه كرر waiting لأن v0.4 استخدم مسار مدير
اللاعبين بدل مؤشر شخصية جيمي الذي تستخدمه الدالة الأصلية PlayerGetPosXYZ.
هذا الإصدار يقرأ المؤشر المباشر [Bully.exe+0x81AEA8] بعد فحص نسخة اللعبة
وتوقيع الدالة، ويتبع مؤشرات التحويل نفسها الموجودة في Bully.exe.

تجربة الإصدار v0.5:
1. ارفع محتويات هذا ZIP إلى جذر مستودع Bully الخاص بك (لا ترفع ZIP نفسه).
   إن كان المشروع مرفوعًا سابقًا، استبدل bridge/dinput8_proxy.cpp
   و .github/workflows/build-bully-position.yml في جذر المستودع.
2. افتح Actions > Build Bully Co-op Position Probe v0.5 (x86) > Run workflow.
3. بعد نجاح البناء افتح Artifacts وحمّل BullyCoop-Hamdan-PositionProbe-v05-x86.
4. أغلق اللعبة، وخذ نسخة احتياطية من dinput8.dll الموجود بجانب Bully.exe.
5. ركّب dinput8.dll المبني (فقط للعبة ذات ملف exe الذي فحصناه).
6. ادخل إلى عالم اللعبة، امشِ مدة 20-30 ثانية، ثم أغلق اللعبة.
7. افتح BullyCoop_bridge.log وفتش عن:
   BullyCoop position v0.5: corrected global-player actor lookup; read-only
   BullyCoop position: x=... y=... z=...
   أو BullyCoop position: waiting: <reason>
   أرسل آخر 15 سطراً لو استمرت المشكلة.

هذا اختبار قراءة إحداثيات فقط؛ لا يوجد لاعب ثانٍ داخل Bully حتى الآن.
لا تغيّر أو تحفظ Bully.exe؛ ولا تستخدم الملف مع لعبة بإصدار مختلف.
لا يمكن ضمان نجاح DLL قبل بنائه وتشغيله على ويندوز.

Proof in submitted Bully.exe disassembly:
  005d267f call 005c7380 with category 3; 005c7395 mov eax,ds:00c1aea8.
  005d2693 read [esi+1554], position via nested+14+30 or actor+14+30.
  005d2687 calls 005db940 manager at 00d02850, but ignores its return.
  v0.4 read the wrong player-manager field for selecting the actor.
