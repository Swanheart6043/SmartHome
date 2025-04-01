import { Space } from 'antd';
import React from 'react';
import { useModel } from 'umi';
import AvatarDropdown from './AvatarDropdown';
import styles from './index.less';
import { ExternalLinks } from './ExternalLinks';

export type SiderTheme = 'light' | 'dark';

const GlobalHeaderRight: React.FC = () => {
  const { initialState } = useModel('@@initialState');

  if (!initialState || !initialState.settings) {
    return null;
  }

  const { navTheme, layout } = initialState.settings;
  let className = styles.right;

  if ((navTheme === 'dark' && layout === 'top') || layout === 'mix') {
    className = `${styles.right}  ${styles.dark}`;
  }

  return (
    <Space className={className}>
      <ExternalLinks />
      <AvatarDropdown menu />
    </Space>
  );
};

export default GlobalHeaderRight;
