# AdrOS
<i><strong>Operating System Unix-like based.</i></strong>

O AdrOs é um kernel baseado em Unix, no momento ele está no início de sua implementacão e apenas imprime uma mensagem na tela, por enquanto, não faz muita coisa além disso.

Pretendo executar códigos escritos em C e adicionar novas funções futuramente.

Para compilar e executar o kernel, será necessário ter intalado em sua máquina o gcc, binutils, make, nasm e qemu.

<ol>
<li>Digite o comando <strong>git clone git://github.com/tadryanom/AdrOS.git</strong> ou baixe diretamente pelo link: https://github.com/tadryanom/AdrOS/archive/master.zip ;</li>
<li>Caso tenha baixado diretamente pelo link como mencionado no passo 1, faça a extração do arquivo e digite <strong>cd Adros</strong>;</li>
<li>Digite <strong>make</strong> para compilar e <strong>make run</strong> para executar, <strong>make clean</strong> serve limpar os binários.</li>
</ol>

Todo o projeto está sendo desenvolvido e compilado em Linux. Para compilar em Windows siga os seguinte passos:
<ol>
  <li> Baixe as ferramentas de compilação pelo link https://github.com/lordmilko/i686-elf-tools/releases/download/7.1.0/i686-elf-tools-windows.zip ;</li>
  <li>Descompacte o arquivo em um local de sua escolha;</li>
  <li>Pressione simultaneamente as teclas <strong>Windows + Pause/Break</strong>;</li>
  <li>Após abrir a janela de Propriedades, clique no link <strong>Configurações avançadas do sistema</strong>;</li>
  <li>Clique na guia <strong>Avançado</strong> e em seguinda clique no botão <strong>Variáveis de Ambiente...</strong>;</li>
  <li>Em <strong>Variáveis de Usuário SEU_USUARIO</strong>, selecione a variável <strong>PATH</strong> e clique no botão <strong>Editar...</strong> ;</li>
  <li>No final do último item, coloque o caracter ";" (ponto-e-virgula, sem as aspas) e coloque o caminho completo onde você descompactou o arquivo no passo 2 acrescido de "<strong>\bin</strong>" (sem aspas). Ex: <strong>C:\Users\USUARIO\i686-elf-tools-windows\bin</strong></li>
  </ol>
  <strong>OBS:</strong> Caso o caminho onde você escolheu por ventura exista ESPAÇOS entre os nomes, coloque o caminho completo entre <strong>""</strong> (aspas). Ex:  <strong>"C:\Users\MEU USUARIO\i686-elf-tools-windows\bin"</strong> .

Thanks!


TODO:

- Construir um gerenciador de memoria
- Obter argumentos passados pelo bootloader
