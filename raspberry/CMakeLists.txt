# Versão mínima do CMake
cmake_minimum_required(VERSION 3.10)

# Nome do seu projeto
project(LineFollower)

# Encontre o pacote OpenCV (obrigatório)
find_package(OpenCV REQUIRED)

# Adicione os diretórios de "include" do OpenCV
include_directories(${OpenCV_INCLUDE_DIRS})

# Crie o executável chamado "meu_programa" a partir do seu arquivo .cpp
add_executable(line_follower line_follower.cpp)

# Linke o seu programa com as bibliotecas do OpenCV
target_link_libraries(line_follower ${OpenCV_LIBS})