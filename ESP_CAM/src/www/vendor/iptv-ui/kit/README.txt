IPTV common UI kit
==================

Только общий слой: стили и компоненты из /common/ui.
Плеер, шапка, пароль и страничный CSS сюда не входят.

Подключение
-----------
<link rel="stylesheet" href="css/page.css">
<link rel="stylesheet" href="css/modal.css">
<script src="js/modal.js"></script>
<script src="js/dropdown.js"></script>
<script src="js/note.js"></script>
<script src="js/seg.js"></script>
<script src="js/time-input.js"></script>
<script src="js/row-dnd.js"></script>

Для диалогов добавьте разметку html/confirm.html и html/prompt.html
(подключают confirm.js / prompt.js сами).

Разметка страницы: <div class="wrap"> … </div>
Сообщения: <div id="note" class="note hidden" role="status"></div>
