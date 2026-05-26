(module
   ;; the below module name is crafted to confuse cli users if a runtime
   ;; outputs it as a part of its "import error" message without escaping.
   (func (import "\1b[2J\1b[H\1b[97;42mHELLO\1b[30;40m" "あいうえおかきくけこ"))
)
