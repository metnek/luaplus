REQUIREMENTS
	lua-5.3 or >, libm, libdialog, libncurses (standard)
	PREFIX=/usr/local/lib/lua/5.3
	CFLAGS+= -I/usr/local/include/lua53 -fsanitize=safe-stack
	LDFLAGS+= -L/usr/local/lib

DESCRIPTION
	libluadialog.so - модуль lua для работы с с диалогами в консоли
	последним агрументом всех функций является таблица с параметрами диалога. доступны следующие параметры:
		title - заголовок окна
		backtitle - заголовок диалога
		ok_b - текст на кнопке OK
		cancel_b - текст на кнопке CANCEL
		extra_b - добавляет кнопку EXTRA, если значение параметра строка то она будет текстом на кнопке, иначе текст по умолчанию
		help_b - добавляет кнопку HELP, если значение параметра строка то она будет текстом на кнопке, иначе текст по умолчанию
		prompt - prompt
		height - высота окна
		width - ширина окна
		list_h - высота формы
		help - есть ли дополнительное описание
		states - статусы для чеклиста


FUNCTIONS
    checklist(table data, table params);
		описание:
			диалог с чеклистом
		параметры:
			data - данные для чеклиста, ключи: name, text, state, help, num
					state - номер статуса. если в параметре params.states ничего не передавать, то state 0 или 1.
			params - параметры диалога
		возврат:
			nil таблица с данными, с дополнительным полем key = значение нажатой кнопки (пример 1)

    radiolist(table data, table params);
		описание:
			диалог с радиолистом
		параметры:
			data - данные для радиолиста, ключи: name, text, state, help, num
					state - номер статуса, 0 или 1.
			params - параметры диалога
		возврат:
			nil таблица с данными, с дополнительным полем key = значение нажатой кнопки (пример 1)

    form(table data, table params);
		описание:
			диалог с формой
		параметры:
			data - данные для чеклиста, ключи: name, name_y, name_x, text, text_y, text_x, ilen, flen, help, num
				name_y - отступ сверху
				name_x - отступ слева
				text_y - отступ сверху
				text_x - отступ слева
				ilen - количество символов
				flen - размер inputa
			params - параметры диалога
		возврат:
			nil таблица с данными, с дополнительным полем key = значение нажатой кнопки

    menu(table data, table params);
		описание:
			диалог с меню
		параметры:
			data - данные для чеклиста, ключи: name, text, help, num
			params - параметры диалога
		возврат:
			nil таблица с выбранными данными с ключом 1, с дополнительным полем key = значение нажатой кнопки

    yesno(table params);
		описание:
			диалог yesno. текст передается в параметре prompt
		параметры:
			params - параметры диалога
		возврат:
			true/false

    msgbox(table params);
		описание:
			диалог msgbox. текст передается в параметре prompt
		параметры:
			params - параметры диалога
		возврат:
			nil

    textbox(string filepath, table params);
		описание:
			диалог textbox.
		параметры:
			filepath - путь до файла
			params - параметры диалога
		возврат:
			nil

EXAMPLES
	Пример 1. checklist();
		Листинг:
			d = require('libluadialog')
			data = {
						[1]={name='kek1', text='kekushka hello', state=0, num=1},
						[2]={name='lol 1', text='lolushka bye', state=2, num=2}
					}
			params = { states=' *+',
						extra_b=true
					 }
			t = d.checklist(data, params)
			for k,v in pairs(t) do
			        if k == 'key' then
		                print(k, v)
			        else
				        print(k)
			                for k1,v1 in pairs(v) do
			                        print('\t',k1,v1)
			                end
			        end
			end

		Вывод:
			1
				name	kek1
				state	0
				text	kekushka hello
				num		1
			2
				name	lol 1
				state	2
				text	lolushka bye
				num		2
			key 3 (нажатая кнопка)
