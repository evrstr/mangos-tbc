# This file is part of the Continued-MaNGOS Project
# 本文件是 Continued-MaNGOS 项目的组成部分
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 本程序为自由软件；您可以根据自由软件基金会发布的 GNU 通用公共许可证条款
# （第 2 版或由您选择的任何更高版本）对本程序进行再发布和/或修改。
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 发布本程序是希望它能发挥作用，但没有任何担保；甚至没有适销性或特定用途适用性的默示担保。
# 详情请参阅 GNU 通用公共许可证。
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
# 您应该已经随本程序收到一份 GNU 通用公共许可证的副本；如果没有，请写信至
# 自由软件基金会：59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

cmake_minimum_required(VERSION 3.16)

INCLUDE(GNUInstallDirs)
find_package(Doxygen)

# 从模板读取并替换变量以生成 Doxyfile
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.dist" DOXYFILE)
string(REPLACE "@PACKAGE@" ${CMAKE_PROJECT_NAME} DOXYFILE_GENERATED "${DOXYFILE}")
string(REPLACE "@VERSION@" ${REVISION_ID} DOXYFILE_GENERATED "${DOXYFILE_GENERATED}")
string(REPLACE "@TOPSRCDIR@" ${CMAKE_SOURCE_DIR} DOXYFILE_GENERATED "${DOXYFILE_GENERATED}")
string(REPLACE "@DESTDIR@" ${CMAKE_BINARY_DIR}/doc DOXYFILE_GENERATED "${DOXYFILE_GENERATED}")
file(WRITE "${CMAKE_BINARY_DIR}/doc/Doxyfile" "${DOXYFILE_GENERATED}")

# 定义 doc 构建目标以运行 Doxygen 生成文档
add_custom_target(
  doc ALL
  COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/doc/Doxyfile &> doxygen.out
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Generating documentation with Doxygen"
)

# 安装生成的 HTML 与 man 手册文档
install(DIRECTORY ${CMAKE_BINARY_DIR}/doc/html DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(DIRECTORY ${CMAKE_BINARY_DIR}/doc/man/man3 DESTINATION ${CMAKE_INSTALL_MANDIR})
