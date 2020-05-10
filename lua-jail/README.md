REQUIREMENTS
	lua-5.3 or >, m, jail kvm util l (standard), kcsbase
	PREFIX=/usr/local/lib/lua/5.3
	CFLAGS+= -I/usr/local/include/lua53 -fsanitize=safe-stack
	LDFLAGS+= -L/usr/local/lib

DESCRIPTION
	libluajail.so - модуль lua для работы с jail

FUNCTIONS
    start(string jail_conf);
		описание:
			создать jail
		параметры:
			jail_conf - путь до файла конфигурации
		возврат:
			nil/true

    stop(string/num jail);
		описание:
			остановить jail
		параметры:
			jail - jid или jailname
		возврат:
			nil/true

    jexec(string/num jail, string cmd);
		описание:
			выполнить команду в jail
		параметры:
			jail - jid или jailname
			cmd - команда
		возврат:
			nil/true

	jls();
		описание:
			получить список jail
		возврат:
			nil или таблица (пример 1)

EXAMPLES
	Пример 1. checklist();
		Листинг:
			jail = require('libluajail')
			t = jail.jls()
			for k,v in pairs(t) do
				print(k);
				for k1,v1 in pairs(v) do
					print("\t", k1, v1)
					end
			end

		Вывод:
			121
				name		st61-001_101_114
				ip			-
				hostname	kekst61-001_101_114
				path		/root/base
