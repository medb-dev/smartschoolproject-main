module.exports = {
  root: true,
  ignorePatterns: ["dist/**", "node_modules/**"],
  env: {
    browser: true,
    es2022: true,
    node: true,
  },
  extends: ["eslint:recommended", "plugin:vue/essential"],
  parserOptions: {
    ecmaVersion: "latest",
    sourceType: "module",
  },
  rules: {
    "no-unused-vars": "off",
    "vue/multi-word-component-names": "off",
  },
};
